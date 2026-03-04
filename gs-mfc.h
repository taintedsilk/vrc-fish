#define KEY_PRESSED(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x0001) ? 1:0) //如果为真，表示按下过
#define KEY_PRESSING(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)  //如果为真，表示正处于按下状态

// PW_RENDERFULLCONTENT may not be defined in older SDKs
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

// 是否按下开始/暂停键（tab）
bool isTab() {
	if (KEY_PRESSED(VK_TAB)) {
		return true;
	}
	return false;
}

// 根据hwnd抓取mat（使用PrintWindow替代StretchBlt，解决DirectX窗口需要resize才能捕获的问题）
Mat hwnd2mat(HWND hwnd, RECT r) {

	if (hwnd == NULL) {
		hwnd = GetDesktopWindow();
	}
	HDC hwindowDC, hwindowCompatibleDC;

	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER bi;

	hwindowDC = GetDC(hwnd);
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

	// 获取窗口客户区大小
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	int clientW = clientRect.right - clientRect.left;
	int clientH = clientRect.bottom - clientRect.top;

	// 创建全客户区大小的位图
	hbwindow = CreateCompatibleBitmap(hwindowDC, clientW, clientH);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = clientW;
	bi.biHeight = -clientH;  // 负值表示自上而下
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	SelectObject(hwindowCompatibleDC, hbwindow);

	// 使用PrintWindow捕获窗口内容（PW_CLIENTONLY只捕获客户区，PW_RENDERFULLCONTENT强制DWM渲染）
	PrintWindow(hwnd, hwindowCompatibleDC, PW_CLIENTONLY | PW_RENDERFULLCONTENT);

	// 提取全客户区到Mat
	Mat fullSrc;
	fullSrc.create(clientH, clientW, CV_8UC4);
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, clientH, fullSrc.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	// 将屏幕坐标转换为客户区坐标并裁剪
	POINT clientOrigin = { 0, 0 };
	ClientToScreen(hwnd, &clientOrigin);
	int cropX = r.left - clientOrigin.x;
	int cropY = r.top - clientOrigin.y;
	int w = r.right - r.left;
	int h = r.bottom - r.top;

	// 边界保护
	if (cropX < 0) cropX = 0;
	if (cropY < 0) cropY = 0;
	if (cropX + w > clientW) w = clientW - cropX;
	if (cropY + h > clientH) h = clientH - cropY;

	if (w > 0 && h > 0) {
		cv::Rect roi(cropX, cropY, w, h);
		src = fullSrc(roi).clone();
	} else {
		src = fullSrc.clone();
	}

	// avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hwnd, hwindowDC);

	return src;
}

// 根据hwnd抓取mat（全客户区）
Mat hwnd2mat(HWND hwnd) {

	if (hwnd == NULL) {
		hwnd = GetDesktopWindow();
	}
	HDC hwindowDC, hwindowCompatibleDC;

	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER bi;

	hwindowDC = GetDC(hwnd);
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

	RECT windowsize;
	GetClientRect(hwnd, &windowsize);

	int w = windowsize.right - windowsize.left;
	int h = windowsize.bottom - windowsize.top;
	src.create(h, w, CV_8UC4);

	hbwindow = CreateCompatibleBitmap(hwindowDC, w, h);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = w;
	bi.biHeight = -h;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	SelectObject(hwindowCompatibleDC, hbwindow);

	// 使用PrintWindow替代StretchBlt
	PrintWindow(hwnd, hwindowCompatibleDC, PW_CLIENTONLY | PW_RENDERFULLCONTENT);

	GetDIBits(hwindowCompatibleDC, hbwindow, 0, h, src.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	// avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hwnd, hwindowDC);

	return src;
}

// 获取原图像
Mat getSrc(HWND hwnd, RECT r) {
	Mat src = hwnd2mat(hwnd, r);
	return matC4ToC3(src);
}
Mat getSrc(RECT r) {
	Mat src = hwnd2mat(NULL, r);
	return matC4ToC3(src);
}
