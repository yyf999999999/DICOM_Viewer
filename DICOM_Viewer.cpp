#include <wx/wx.h>
#include <wx/slider.h>
#include <wx/choice.h>
#include <wx/dir.h>
#include <wx/progdlg.h>
#include <wx/dcbuffer.h>
#include <wx/splitter.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <functional>

// DCMTK headers
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmimgle/dcmimage.h"

// --- 定数カラー定義 ---
const wxColour COL_AXIAL(255, 50, 50);     // Red
const wxColour COL_CORONAL(50, 255, 50);   // Green
const wxColour COL_SAGITTAL(50, 100, 255); // Blue

// --- 描画用パネル ---
class ImagePanel : public wxPanel {
    wxBitmap displayedBitmap;
    int viewType; // 0:Axial, 1:Coronal, 2:Sagittal
    double crossX = -1.0;
    double crossY = -1.0;
    bool isJapanese = false; // デフォルト英語

    wxColour borderColor;
    wxColour vLineColor;
    wxColour hLineColor;

    std::function<void(int)> onClickCallback;
    std::function<void(int, int)> onWheelCallback;

public:
    ImagePanel(wxWindow* parent, int type, 
               std::function<void(int)> clickCb, 
               std::function<void(int, int)> wheelCb) 
        : wxPanel(parent, wxID_ANY), viewType(type), onClickCallback(clickCb), onWheelCallback(wheelCb)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(wxColour(20, 20, 20)); 

        switch(viewType) {
            case 0: // Axial
                borderColor = COL_AXIAL;
                vLineColor = COL_SAGITTAL; hLineColor = COL_CORONAL;
                break;
            case 1: // Coronal
                borderColor = COL_CORONAL;
                vLineColor = COL_SAGITTAL; hLineColor = COL_AXIAL;
                break;
            case 2: // Sagittal
                borderColor = COL_SAGITTAL;
                vLineColor = COL_CORONAL; hLineColor = COL_AXIAL;
                break;
        }

        Bind(wxEVT_PAINT, &ImagePanel::OnPaint, this);
        Bind(wxEVT_SIZE, &ImagePanel::OnSize, this);
        Bind(wxEVT_LEFT_DOWN, &ImagePanel::OnMouseClick, this);
        Bind(wxEVT_MOUSEWHEEL, &ImagePanel::OnMouseWheel, this);
    }

    void SetLanguage(bool jp) {
        isJapanese = jp;
        Refresh(false);
    }

    void SetImage(const wxImage& img, double cx, double cy) {
        if (!img.IsOk()) return;
        displayedBitmap = wxBitmap(img);
        crossX = cx;
        crossY = cy;
        Refresh(false);
        Update();
    }

    void OnMouseClick(wxMouseEvent& evt) {
        SetFocus();
        if (onClickCallback) onClickCallback(viewType);
        evt.Skip();
    }

    void OnMouseWheel(wxMouseEvent& evt) {
        if (onWheelCallback) {
            int rotation = evt.GetWheelRotation();
            int direction = (rotation > 0) ? 1 : -1;
            onWheelCallback(viewType, direction);
        }
        evt.Skip();
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();

        wxSize panelSize = GetClientSize();
        int x = 0, y = 0, w = 0, h = 0;
        if (displayedBitmap.IsOk()) {
            wxSize bmpSize = displayedBitmap.GetSize();
            w = bmpSize.x; h = bmpSize.y;
            x = (panelSize.x - w) / 2; y = (panelSize.y - h) / 2;
            if (x < 0) x = 0; if (y < 0) y = 0;
            dc.DrawBitmap(displayedBitmap, x, y, false);
        }

        wxPen borderPen(borderColor, 3, wxPENSTYLE_SOLID);
        dc.SetPen(borderPen);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(1, 1, panelSize.x - 2, panelSize.y - 2);

        if (displayedBitmap.IsOk() && crossX >= 0 && crossY >= 0) {
            int lineX = x + (int)(w * crossX);
            int lineY = y + (int)(h * crossY);
            if (lineX >= x && lineX < x + w && lineY >= y && lineY < y + h) {
                dc.SetPen(wxPen(vLineColor, 1, wxPENSTYLE_SOLID));
                dc.DrawLine(lineX, y, lineX, y + h);
                dc.SetPen(wxPen(hLineColor, 1, wxPENSTYLE_SOLID));
                dc.DrawLine(x, lineY, x + w, lineY);
            }
        }
        
        dc.SetFont(wxFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        wxString label;
        if (isJapanese) {
            switch(viewType) {
                case 0: label = L"Axial (上から)"; break;
                case 1: label = L"Coronal (正面から)"; break;
                case 2: label = L"Sagittal (横から)"; break;
            }
        } else {
            switch(viewType) {
                case 0: label = L"Axial (Top)"; break;
                case 1: label = L"Coronal (Front)"; break;
                case 2: label = L"Sagittal (Side)"; break;
            }
        }

        dc.SetTextForeground(*wxBLACK);
        dc.DrawText(label, 11, 11); 
        dc.SetTextForeground(borderColor);
        dc.DrawText(label, 10, 10);
    }

    void OnSize(wxSizeEvent& evt) {
        Refresh(false);
        evt.Skip();
    }
};

class MainFrame : public wxFrame {
public:
    MainFrame() : wxFrame(NULL, wxID_ANY, L"DICOM Viewer", wxDefaultPosition, wxSize(1280, 900)) {
        SetBackgroundColour(wxColour(30, 30, 30));

        // --- Menu Bar ---
        wxMenuBar* menuBar = new wxMenuBar();
        
        wxMenu* fileMenu = new wxMenu();
        fileMenu->Append(wxID_OPEN, L"Open Folder");
        fileMenu->Append(wxID_EXIT, L"Exit");
        menuBar->Append(fileMenu, L"File");

        wxMenu* viewMenu = new wxMenu();
        viewMenu->Append(1010, L"Show/Hide Controls\tF11");
        menuBar->Append(viewMenu, L"View");

        wxMenu* langMenu = new wxMenu();
        langMenu->AppendRadioItem(1001, L"English");
        langMenu->AppendRadioItem(1002, L"日本語");
        langMenu->Check(1001, true); 
        menuBar->Append(langMenu, L"Language");

        SetMenuBar(menuBar);
        
        Bind(wxEVT_MENU, &MainFrame::OnLoadBtn, this, wxID_OPEN);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ Close(); }, wxID_EXIT);
        Bind(wxEVT_MENU, &MainFrame::OnLanguageChange, this, 1001);
        Bind(wxEVT_MENU, &MainFrame::OnLanguageChange, this, 1002);
        Bind(wxEVT_MENU, &MainFrame::OnToggleControls, this, 1010);

        // --- Layout ---
        rootSizer = new wxBoxSizer(wxHORIZONTAL);
        imageAreaSizer = new wxBoxSizer(wxVERTICAL);
        bottomSizer = new wxBoxSizer(wxHORIZONTAL);

        auto clickCb = [this](int type){ this->SwitchLayout(type); };
        auto wheelCb = [this](int type, int dir){ this->OnPanelWheel(type, dir); };

        panelAxial = new ImagePanel(this, 0, clickCb, wheelCb);
        panelCoronal = new ImagePanel(this, 1, clickCb, wheelCb);
        panelSagittal = new ImagePanel(this, 2, clickCb, wheelCb);

        SwitchLayout(0);
        rootSizer->Add(imageAreaSizer, 1, wxEXPAND | wxALL, 0);

        // --- Control Panel ---
        sidePanel = new wxPanel(this, wxID_ANY);
        sidePanel->SetBackgroundColour(wxColour(40, 40, 40));
        sidePanel->SetMinSize(wxSize(320, -1));
        
        wxBoxSizer* sideSizer = new wxBoxSizer(wxVERTICAL);

        loadBtn = new wxButton(sidePanel, wxID_ANY, L"Open Folder");
        sideSizer->Add(loadBtn, 0, wxEXPAND | wxALL, 10);

        infoText = new wxTextCtrl(sidePanel, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 120), wxTE_MULTILINE | wxTE_READONLY);
        infoText->SetBackgroundColour(*wxWHITE);
        infoText->SetForegroundColour(*wxBLACK);
        sideSizer->Add(infoText, 0, wxEXPAND | wxALL, 10);

        // Sliders
        labelZ = new wxStaticText(sidePanel, wxID_ANY, ""); 
        ConfigureLabel(labelZ, COL_AXIAL); sideSizer->Add(labelZ, 0, wxLEFT | wxTOP, 10);
        
        sliderZ = new wxSlider(sidePanel, wxID_ANY, 0, 0, 1, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
        sliderZ->SetForegroundColour(*wxWHITE); sideSizer->Add(sliderZ, 0, wxEXPAND | wxALL, 5);

        labelY = new wxStaticText(sidePanel, wxID_ANY, ""); 
        ConfigureLabel(labelY, COL_CORONAL); sideSizer->Add(labelY, 0, wxLEFT | wxTOP, 10);

        sliderY = new wxSlider(sidePanel, wxID_ANY, 0, 0, 1, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
        sliderY->SetForegroundColour(*wxWHITE); sideSizer->Add(sliderY, 0, wxEXPAND | wxALL, 5);

        labelX = new wxStaticText(sidePanel, wxID_ANY, ""); 
        ConfigureLabel(labelX, COL_SAGITTAL); sideSizer->Add(labelX, 0, wxLEFT | wxTOP, 10);

        sliderX = new wxSlider(sidePanel, wxID_ANY, 0, 0, 1, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
        sliderX->SetForegroundColour(*wxWHITE); sideSizer->Add(sliderX, 0, wxEXPAND | wxALL, 5);

        // Quality
        labelWL = new wxStaticText(sidePanel, wxID_ANY, "");
        ConfigureLabel(labelWL, *wxWHITE); sideSizer->Add(labelWL, 0, wxLEFT | wxTOP, 20);

        wlSlider = new wxSlider(sidePanel, wxID_ANY, 40, -1000, 3000, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
        wlSlider->SetForegroundColour(*wxWHITE);
        wwSlider = new wxSlider(sidePanel, wxID_ANY, 400, 1, 4000, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
        wwSlider->SetForegroundColour(*wxWHITE);
        sideSizer->Add(wlSlider, 0, wxEXPAND | wxALL, 5);
        sideSizer->Add(wwSlider, 0, wxEXPAND | wxALL, 5);
        
        // リセットボタン (最初から有効化 + 黒文字)
        resetBtn = new wxButton(sidePanel, wxID_ANY, L"Reset");
        resetBtn->SetForegroundColour(*wxBLACK); // 文字色を黒に
        resetBtn->Enable(true); // ★修正：最初から有効化
        sideSizer->Add(resetBtn, 0, wxEXPAND | wxALL, 15);

        hintLabel = new wxStaticText(sidePanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        hintLabel->SetForegroundColour(wxColour(200, 200, 200));
        sideSizer->Add(hintLabel, 0, wxALL | wxEXPAND, 15);

        sidePanel->SetSizer(sideSizer);
        rootSizer->Add(sidePanel, 0, wxEXPAND | wxALL, 0); 
        SetSizer(rootSizer);

        // Binds
        loadBtn->Bind(wxEVT_BUTTON, &MainFrame::OnLoadBtn, this);
        resetBtn->Bind(wxEVT_BUTTON, &MainFrame::OnResetBtn, this);

        auto BindS = [&](wxSlider* s, void (MainFrame::*f)(wxCommandEvent&), void (MainFrame::*g)(wxScrollEvent&)) {
            s->Bind(wxEVT_SLIDER, f, this);
            s->Bind(wxEVT_SCROLL_THUMBTRACK, g, this);
        };
        BindS(sliderX, &MainFrame::OnSliceChange, &MainFrame::OnSliceChangeRaw);
        BindS(sliderY, &MainFrame::OnSliceChange, &MainFrame::OnSliceChangeRaw);
        BindS(sliderZ, &MainFrame::OnSliceChange, &MainFrame::OnSliceChangeRaw);
        BindS(wlSlider, &MainFrame::OnSliceChange, &MainFrame::OnSliceChangeRaw);
        BindS(wwSlider, &MainFrame::OnSliceChange, &MainFrame::OnSliceChangeRaw);

        EnableControls(false); 
        // ★修正: リセットボタンだけは常に有効にしておく（ガード処理済み）
        resetBtn->Enable(true); 
        
        UpdateUIText(); 
    }

private:
    struct SliceRaw { int instance; std::vector<int16_t> pixels; };
    std::vector<int16_t> volumeData;
    int volWidth = 0, volHeight = 0, volDepth = 0;
    double pxSpcX = 1.0, pxSpcY = 1.0, sliceThick = 1.0;
    bool isJapanese = false; 

    wxString patientName = "Unknown";
    wxString patientID = "Unknown";

    wxBoxSizer *rootSizer, *imageAreaSizer, *bottomSizer;
    wxPanel* sidePanel;
    ImagePanel *panelAxial, *panelCoronal, *panelSagittal;
    
    wxButton *loadBtn, *resetBtn;
    wxTextCtrl* infoText;
    wxStaticText *labelX, *labelY, *labelZ, *labelWL, *hintLabel;
    wxSlider *sliderX, *sliderY, *sliderZ, *wlSlider, *wwSlider;

    void ConfigureLabel(wxStaticText* t, const wxColour& col) {
        t->SetForegroundColour(col);
        t->SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    }

    void OnToggleControls(wxCommandEvent&) {
        if (sidePanel->IsShown()) sidePanel->Hide();
        else sidePanel->Show();
        rootSizer->Layout();
    }

    void OnLanguageChange(wxCommandEvent& evt) {
        if (evt.GetId() == 1001) isJapanese = false;
        else isJapanese = true;
        UpdateUIText();
    }

    // リセット処理 (データがないときは何もしない安全装置付き)
    void OnResetBtn(wxCommandEvent&) {
        if (volumeData.empty()) return;
        
        sliderX->SetValue(sliderX->GetMax() / 2);
        sliderY->SetValue(sliderY->GetMax() / 2);
        sliderZ->SetValue(sliderZ->GetMax() / 2);

        wlSlider->SetValue(40);
        wwSlider->SetValue(400);

        UpdateAllViews();
    }

    void UpdateUIText() {
        panelAxial->SetLanguage(isJapanese);
        panelCoronal->SetLanguage(isJapanese);
        panelSagittal->SetLanguage(isJapanese);

        if (isJapanese) {
            SetTitle(L"DICOM ビューアー");
            loadBtn->SetLabel(L"フォルダを開く");
            resetBtn->SetLabel(L"リセット");
            infoText->SetValue(infoText->GetValue().IsEmpty() ? L"データなし" : GetInfoString());
            labelZ->SetLabel(L"Axial 位置 (Z) - 赤枠");
            labelY->SetLabel(L"Coronal 位置 (Y) - 緑枠");
            labelX->SetLabel(L"Sagittal 位置 (X) - 青枠");
            labelWL->SetLabel(L"ウィンドウレベル / 幅 (明るさ・コントラスト)");
            hintLabel->SetLabel(L"ヒント: 下の画像をクリックすると\n上のメイン画面と入れ替わります");
        } else {
            SetTitle(L"DICOM Viewer");
            loadBtn->SetLabel(L"Open Folder");
            resetBtn->SetLabel(L"Reset");
            infoText->SetValue(infoText->GetValue().IsEmpty() ? L"No Data" : GetInfoString());
            labelZ->SetLabel(L"Axial Slice (Z) - Red Frame");
            labelY->SetLabel(L"Coronal Slice (Y) - Green Frame");
            labelX->SetLabel(L"Sagittal Slice (X) - Blue Frame");
            labelWL->SetLabel(L"Window Level / Width");
            hintLabel->SetLabel(L"Hint: Click a bottom image to\nswap it with the main view.");
        }
        Layout();
    }

    wxString GetInfoString() {
        std::stringstream ss;
        if (isJapanese) {
            ss << "名前: " << patientName << "\r\nID: " << patientID << "\r\n";
            ss << "サイズ: " << volWidth << " x " << volHeight << "\r\n";
            ss << "スライス数: " << volDepth << "\r\n";
            ss << "スライス厚: " << sliceThick << " mm";
        } else {
            ss << "Name: " << patientName << "\r\nID: " << patientID << "\r\n";
            ss << "Size: " << volWidth << " x " << volHeight << "\r\n";
            ss << "Slices: " << volDepth << "\r\n";
            ss << "Thickness: " << sliceThick << " mm";
        }
        return wxString::FromUTF8(ss.str().c_str());
    }

    void SwitchLayout(int mainViewType) {
        imageAreaSizer->Detach(bottomSizer);
        imageAreaSizer->Clear(false); bottomSizer->Clear(false);
        ImagePanel *mainP=nullptr, *sub1=nullptr, *sub2=nullptr;

        if (mainViewType == 0) { mainP = panelAxial; sub1 = panelCoronal; sub2 = panelSagittal; }
        else if (mainViewType == 1) { mainP = panelCoronal; sub1 = panelAxial; sub2 = panelSagittal; }
        else { mainP = panelSagittal; sub1 = panelAxial; sub2 = panelCoronal; }

        imageAreaSizer->Add(mainP, 3, wxEXPAND | wxALL, 2);
        bottomSizer->Add(sub1, 1, wxEXPAND | wxALL, 2);
        bottomSizer->Add(sub2, 1, wxEXPAND | wxALL, 2);
        imageAreaSizer->Add(bottomSizer, 2, wxEXPAND | wxALL, 2);
        Layout(); Refresh();
    }

    void OnPanelWheel(int viewType, int direction) {
        if (volumeData.empty()) return;
        wxSlider* targetSlider = nullptr;
        if (viewType == 0) targetSlider = sliderZ; 
        else if (viewType == 1) targetSlider = sliderY; 
        else targetSlider = sliderX; 

        if (targetSlider) {
            int val = targetSlider->GetValue();
            val += direction;
            if (val < targetSlider->GetMin()) val = targetSlider->GetMin();
            if (val > targetSlider->GetMax()) val = targetSlider->GetMax();
            targetSlider->SetValue(val);
            UpdateAllViews(); 
        }
    }

    void EnableControls(bool enable) {
        sliderX->Enable(enable); sliderY->Enable(enable); sliderZ->Enable(enable);
        wlSlider->Enable(enable); wwSlider->Enable(enable);
        // resetBtnは常に有効なのでここでは触らない
    }

    void OnLoadBtn(wxCommandEvent&) {
        wxDirDialog dlg(this, isJapanese ? L"DICOMフォルダを選択" : L"Select DICOM Folder");
        if (dlg.ShowModal() != wxID_OK) return;
        wxArrayString files;
        wxDir::GetAllFiles(dlg.GetPath(), &files, "*.dcm", wxDIR_FILES);
        if (files.IsEmpty()) return;

        std::map<std::string, std::vector<std::string>> seriesMap;
        wxProgressDialog scanner(isJapanese ? L"スキャン中" : L"Scanning", 
                                 isJapanese ? L"シリーズを分類しています..." : L"Grouping Series...", 
                                 files.GetCount(), this, wxPD_APP_MODAL | wxPD_AUTO_HIDE);
        for(size_t i=0; i<files.GetCount(); ++i) {
            DcmFileFormat ff;
            if(ff.loadFile(files[i].ToStdString().c_str(), EXS_Unknown, EGL_noChange, DCM_MaxReadLength, ERM_autoDetect).good()) {
                const char* uid = nullptr;
                ff.getDataset()->findAndGetString(DCM_SeriesInstanceUID, uid);
                if(uid) seriesMap[uid].push_back(files[i].ToStdString());
            }
            if(i%20==0) scanner.Update(i);
        }
        if(seriesMap.empty()) return;

        std::string bestUID; size_t maxCount = 0;
        for(auto const& [uid, list] : seriesMap) {
            if(list.size() > maxCount) { maxCount = list.size(); bestUID = uid; }
        }

        const auto& targetFiles = seriesMap[bestUID];
        std::vector<SliceRaw> tempSlices;
        scanner.Update(0, isJapanese ? L"画像データを読み込んでいます..." : L"Loading Pixel Data...");
        scanner.SetRange(targetFiles.size());

        volWidth = 0; volHeight = 0;
        
        for(size_t i=0; i<targetFiles.size(); ++i) {
            DcmFileFormat ff;
            if(ff.loadFile(targetFiles[i].c_str()).good()) {
                DcmDataset* ds = ff.getDataset();
                if(volWidth == 0) {
                    Uint16 w, h;
                    ds->findAndGetUint16(DCM_Rows, h); ds->findAndGetUint16(DCM_Columns, w);
                    volWidth = w; volHeight = h;
                    const Float64* sp = nullptr;
                    if(ds->findAndGetFloat64Array(DCM_PixelSpacing, sp).good()) { pxSpcY = sp[0]; pxSpcX = sp[1]; }
                    ds->findAndGetFloat64(DCM_SliceThickness, sliceThick);
                    const char* tmp=nullptr;
                    if(ds->findAndGetString(DCM_PatientName, tmp).good() && tmp) patientName=wxString::FromUTF8(tmp);
                    if(ds->findAndGetString(DCM_PatientID, tmp).good() && tmp) patientID=wxString::FromUTF8(tmp);
                }
                DicomImage img(ds, EXS_Unknown);
                if(img.getStatus() == EIS_Normal && img.getWidth()==volWidth && img.getHeight()==volHeight) {
                    const int16_t* data = (const int16_t*)img.getOutputData(16);
                    if(data) {
                        Sint32 inst = 0;
                        ds->findAndGetSint32(DCM_InstanceNumber, inst);
                        std::vector<int16_t> buf(data, data + (volWidth * volHeight));
                        tempSlices.push_back({(int)inst, buf});
                    }
                }
            }
            if(i%10==0) scanner.Update(i);
        }
        if(tempSlices.empty()) return;
        std::sort(tempSlices.begin(), tempSlices.end(), [](const SliceRaw& a, const SliceRaw& b){ return a.instance < b.instance; });

        volDepth = tempSlices.size();
        volumeData.clear();
        volumeData.reserve((size_t)volWidth * volHeight * volDepth);
        for(const auto& s : tempSlices) volumeData.insert(volumeData.end(), s.pixels.begin(), s.pixels.end());

        infoText->SetValue(GetInfoString());

        sliderX->SetRange(0, volWidth - 1); sliderX->SetValue(volWidth / 2);
        sliderY->SetRange(0, volHeight - 1); sliderY->SetValue(volHeight / 2);
        sliderZ->SetRange(0, volDepth - 1); sliderZ->SetValue(volDepth / 2);

        EnableControls(true);
        UpdateAllViews();
    }

    void OnSliceChange(wxCommandEvent&) { UpdateAllViews(); }
    void OnSliceChangeRaw(wxScrollEvent&) { UpdateAllViews(); }

    void UpdateAllViews() {
        if(volumeData.empty()) return;
        int curX = sliderX->GetValue();
        int curY = sliderY->GetValue();
        int curZ = sliderZ->GetValue();
        int wl = wlSlider->GetValue();
        int ww = wwSlider->GetValue();

        UpdateOneView(panelAxial, 0, curZ, curX, curY, wl, ww);
        UpdateOneView(panelCoronal, 1, curY, curX, curZ, wl, ww);
        UpdateOneView(panelSagittal, 2, curX, curY, curZ, wl, ww);
    }

    void UpdateOneView(ImagePanel* panel, int viewType, int sliceIdx, int cross1, int cross2, int wl, int ww) {
        int w = 0, h = 0;
        double scaleY = 1.0;
        std::vector<int16_t> buf;
        double sx = (pxSpcX > 0) ? pxSpcX : 1.0;
        double sy = (pxSpcY > 0) ? pxSpcY : 1.0;
        double sz = (sliceThick > 0) ? sliceThick : 1.0;

        if (viewType == 0) { 
            w = volWidth; h = volHeight; scaleY = sy / sx;
            size_t off = (size_t)sliceIdx * w * h;
            if(off + w*h <= volumeData.size()) buf.assign(volumeData.begin() + off, volumeData.begin() + off + w*h);
        } else if (viewType == 1) { 
            w = volWidth; h = volDepth; scaleY = sz / sx;
            buf.resize(w * h);
            for (int z = 0; z < h; ++z) {
                size_t base = (size_t)z * (volWidth * volHeight) + (size_t)sliceIdx * volWidth;
                if (base + w <= volumeData.size()) std::copy(volumeData.begin() + base, volumeData.begin() + base + w, buf.begin() + z * w);
            }
        } else { 
            w = volHeight; h = volDepth; scaleY = sz / sy;
            buf.resize(w * h);
            for (int z = 0; z < h; ++z) {
                for (int y = 0; y < w; ++y) {
                    size_t idx = (size_t)z * (volWidth * volHeight) + (size_t)y * volWidth + sliceIdx;
                    if(idx < volumeData.size()) buf[z * w + y] = volumeData[idx];
                }
            }
        }

        if(buf.empty()) return;
        wxImage img(w, h);
        unsigned char* rgb = img.GetData();
        if(ww < 1) ww = 1;
        double lower = wl - ww / 2.0;
        double range = ww;

        for(int i=0; i<w*h; ++i) {
            double val = buf[i];
            if(val <= lower) val = 0;
            else if(val >= lower + range) val = 255;
            else val = ((val - lower) / range) * 255.0;
            unsigned char p = (unsigned char)val;
            rgb[i*3] = p; rgb[i*3+1] = p; rgb[i*3+2] = p;
        }

        int finalW = w, finalH = (int)(h * scaleY);
        double maxDim = 800.0;
        if(finalW > maxDim || finalH > maxDim) {
            double s = std::min(maxDim/finalW, maxDim/finalH);
            finalW = (int)(finalW * s); finalH = (int)(finalH * s);
        }
        if(finalW < 1) finalW=1; if(finalH < 1) finalH=1;
        img.Rescale(finalW, finalH, wxIMAGE_QUALITY_HIGH);

        double relX = (double)cross1 / (w - 1);
        double relY = (double)cross2 / (h - 1);
        panel->SetImage(img, relX, relY);
    }
};

class App : public wxApp {
public:
    bool OnInit() {
        wxInitAllImageHandlers();
        (new MainFrame())->Show();
        return true;
    }
};
wxIMPLEMENT_APP(App);