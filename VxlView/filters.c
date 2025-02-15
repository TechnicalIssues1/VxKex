#include "vxlview.h"
#include "resource.h"

STATIC CONST USHORT FilterControlIds[] = {
	IDC_GBFILTERSEVERITY,
	IDC_CBFILTERCRITICAL,
	IDC_CBFILTERERROR,
	IDC_CBFILTERWARNING,
	IDC_CBFILTERINFORMATION,
	IDC_CBFILTERDETAIL,
	IDC_CBFILTERDEBUG
};

VOID InitializeFilterControls(
	VOID)
{
	ULONG Index;
	HRESULT Result;
	WCHAR ToolTipText[256];

	CreateDialog(NULL, MAKEINTRESOURCE(IDD_FILTERS), MainWindow, FilterWndProc);

	// add tool-tips to the checkboxes
	for (Index = IDC_CBFILTERCRITICAL; Index < (IDC_CBFILTERCRITICAL + LogSeverityMaximumValue); Index++) {
		Result = StringCchPrintf(
			ToolTipText,
			ARRAYSIZE(ToolTipText),
			L"Display %s events.",
			VxlSeverityToText((VXLSEVERITY) (Index - IDC_CBFILTERCRITICAL), FALSE));

		if (SUCCEEDED(Result)) {
			ToolTip(FilterWindow, Index, ToolTipText);
		}
	}

	// set cue text in the search box and add a tool-tip to relevant items
	Edit_SetCueBannerText(GetDlgItem(FilterWindow, IDC_SEARCHBOX), L"Enter search term...");
	
	ToolTip(FilterWindow, IDC_SEARCHBOX, L"Search log entry message text.");
	
	ToolTip(FilterWindow, IDC_CBCASESENSITIVE, L"When checked, consider text with different capitalization to be different.\r\n\r\n"
											   L"For example, the search term \"Carrot\" would not match a log entry which contains \"CARROT\".");
	
	ToolTip(FilterWindow, IDC_CBWILDCARD, L"When checked, allow wild-card matching with * (match any sequence) and ? (match any character).\r\n\r\n"
										  L"For example, the search term \"?o?ato\" would match log entries containing both \"potato\" and \"tomato\".");
	
	ToolTip(FilterWindow, IDC_CBINVERTSEARCH, L"When checked, only show results that do NOT match the search criteria.");

	ToolTip(FilterWindow, IDC_CBEXACTMATCH, L"When checked, do not show results with leading and trailing text.\r\n\r\n"
											L"For example, the search term \"Application started\" would not match a log entry which has the text \"Application started!\" due to the extra exclamation mark.");

	ToolTip(FilterWindow, IDC_CBSEARCHWHOLE, L"When checked, search the entire log text (including details), not just "
											 L"what is displayed in the list view.");

	// initialize component selection list view
	ListView_SetExtendedListViewStyle(
		GetDlgItem(FilterWindow, IDC_COMPONENTLIST),
		LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);

	ResetFilterControls();
}

VOID ResetFilterControls(
	VOID)
{
	ULONG Index;

	for (Index = IDC_CBFILTERCRITICAL; Index < (IDC_CBFILTERCRITICAL + LogSeverityMaximumValue); Index++) {
		CheckDlgButton(FilterWindow, Index, TRUE);
	}

	for (Index = IDC_CBCASESENSITIVE; Index <= IDC_CBEXACTMATCH; Index++) {
		CheckDlgButton(FilterWindow, Index, FALSE);
	}

	SetDlgItemText(FilterWindow, IDC_SEARCHBOX, L"");
	ListView_SetCheckedStateAll(GetDlgItem(FilterWindow, IDC_COMPONENTLIST), TRUE);
}

VOID ResizeFilterControls(
	VOID)
{
	RECT StatusBarWindowRect;
	RECT FilterWindowClientRect;

	GetClientRect(FilterWindow, &FilterWindowClientRect);
	GetWindowRect(GetDlgItem(MainWindow, IDC_STATUSBAR), &StatusBarWindowRect);
	MapWindowPoints(HWND_DESKTOP, MainWindow, (PPOINT) &StatusBarWindowRect, 2);

	//
	// Put the bottom of the filter window 10px above the top of the status bar.
	//
	SetWindowPos(
		FilterWindow,
		NULL,
		15, StatusBarWindowRect.top - FilterWindowClientRect.bottom - 10,
		0, 0,
		SWP_NOSIZE | SWP_NOZORDER);
}

VOID BuildBackendFilters(
	OUT	PBACKENDFILTERS	Filters)
{
	ULONG Index;
	ULONG NumberOfComponents;
	ULONG TextFilterBufCch;
	BOOLEAN NeedToAddTwoStars;
	HWND ComponentListWindow;
	STATIC PWSTR TextFilter = NULL;

	SafeFree(TextFilter);

	TextFilterBufCch = GetWindowTextLength(GetDlgItem(FilterWindow, IDC_SEARCHBOX)) + 1;

	if (!IsDlgButtonChecked(FilterWindow, IDC_CBEXACTMATCH) &&
		IsDlgButtonChecked(FilterWindow, IDC_CBWILDCARD)) {

		// add two '*' characters to beginning and end to implement non-exact match
		TextFilterBufCch += 2;
		NeedToAddTwoStars = TRUE;
	} else {
		NeedToAddTwoStars = FALSE;
	}

	Filters->TextFilterCaseSensitive = IsDlgButtonChecked(FilterWindow, IDC_CBCASESENSITIVE);
	Filters->TextFilterWildcardMatch = IsDlgButtonChecked(FilterWindow, IDC_CBWILDCARD);
	Filters->TextFilterInverted = IsDlgButtonChecked(FilterWindow, IDC_CBINVERTSEARCH);
	Filters->TextFilterExact = IsDlgButtonChecked(FilterWindow, IDC_CBEXACTMATCH);
	Filters->TextFilterWhole = IsDlgButtonChecked(FilterWindow, IDC_CBSEARCHWHOLE);

	TextFilter = SafeAlloc(WCHAR, TextFilterBufCch);
	if (TextFilter) {
		GetDlgItemText(
			FilterWindow,
			IDC_SEARCHBOX,
			TextFilter + NeedToAddTwoStars,
			TextFilterBufCch - (NeedToAddTwoStars * 2));

		if (NeedToAddTwoStars) {
			TextFilter[0] = '*';
			TextFilter[TextFilterBufCch - 2] = '*';
			TextFilter[TextFilterBufCch - 1] = '\0';
		}

		RtlInitUnicodeString(&Filters->TextFilter, TextFilter);

		if (Filters->TextFilterWildcardMatch && !Filters->TextFilterCaseSensitive) {
			// needed for RtlIsNameInExpression
			RtlUpcaseUnicodeString(&Filters->TextFilter, &Filters->TextFilter, FALSE);
		}
	}

	for (Index = IDC_CBFILTERCRITICAL; Index <= IDC_CBFILTERDEBUG; Index++) {
		VXLSEVERITY Severity;

		Severity = (VXLSEVERITY) (Index - IDC_CBFILTERCRITICAL);
		Filters->SeverityFilters[Severity] = IsDlgButtonChecked(FilterWindow, Index);
	}

	ComponentListWindow = GetDlgItem(FilterWindow, IDC_COMPONENTLIST);
	NumberOfComponents = ListView_GetItemCount(ComponentListWindow);

	for (Index = 0; Index < NumberOfComponents; ++Index) {
		Filters->ComponentFilters[Index] = ListView_GetCheckState(
			ComponentListWindow, 
			ListView_MapIDToIndex(ComponentListWindow, Index));
	}
}

VOID UpdateFilters(
	VOID)
{
	BACKENDFILTERS Filters;

	if (!IsLogFileOpened()) {
		return;
	}

	BuildBackendFilters(&Filters);
	SetBackendFilters(&Filters);
}

INT_PTR CALLBACK FilterWndProc(
	IN	HWND	_FilterWindow,
	IN	UINT	Message,
	IN	WPARAM	WParam,
	IN	LPARAM	LParam)
{
	STATIC BOOLEAN FirstTime;

	if (Message == WM_INITDIALOG) {
		UNCONST (HWND) FilterWindow = _FilterWindow;
		FirstTime = TRUE;
	} else if (Message == WM_CONTEXTMENU) {
		HWND Window;
		POINT ClickPoint;

		Window = (HWND) WParam;
		ClickPoint.x = GET_X_LPARAM(LParam);
		ClickPoint.y = GET_Y_LPARAM(LParam);

		if (Window == GetDlgItem(FilterWindow, IDC_COMPONENTLIST)) {
			ULONG Selection;

			//
			// display the "select all/select none" menu on the components
			// list view
			//
			Selection = ContextMenu(Window, IDM_COMPONENTLISTMENU, &ClickPoint);

			if (Selection == M_SELECTALL || Selection == M_SELECTNONE) {
				ListView_SetCheckedStateAll(Window, (Selection == M_SELECTALL));
			}
		}
	} else if (Message == WM_COMMAND) {
		USHORT NotificationId;
		WCHAR ClassName[32];

		GetClassName((HWND) LParam, ClassName, ARRAYSIZE(ClassName));
		NotificationId = HIWORD(WParam);

		if (StringEqual(ClassName, L"Edit") && NotificationId != EN_CHANGE) {
			// don't care about any other edit notifications besides
			// EN_CHANGE
			return FALSE;
		}

		UpdateFilters();
	} else if (Message == WM_NOTIFY) {
		LPNMHDR Notification;

		Notification = (LPNMHDR) LParam;

		if (Notification->idFrom == IDC_COMPONENTLIST) {
			if (Notification->code == LVN_ITEMCHANGED) {
				LPNMLISTVIEW ChangedItemInfo;

				ChangedItemInfo = (LPNMLISTVIEW) LParam;

				if (ChangedItemInfo->uNewState & 0x3000) {
					if (FirstTime) {
						FirstTime = FALSE;
					} else {
						// An source component was either checked or unchecked
						UpdateFilters();
					}
				}
			} else {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}

	return TRUE;
}