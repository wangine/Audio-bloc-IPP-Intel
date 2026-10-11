#pragma once
#include <QWidget>
namespace entropy::host
{
inline void applySourceStyle(QWidget &window)
{
    window.setStyleSheet(R"(
      QWidget { background: #071722; color: #d7e5f2; font-family: "Segoe UI"; font-size: 12px; }
      QMainWindow { background: #06131d; }
      QLabel#brand { font-size: 32px; font-weight: 600; letter-spacing: 7px; color: #f4f8fb; }
      QLabel#subtitle { color: #8da9bd; letter-spacing: 3px; font-size: 9px; }
      QLabel#inputTitle { font-size:17px; font-weight:700; color:#f0f6fb; }
      QLabel#inputState { font-size:11px; font-weight:700; }
      QSpinBox {background:#071822;border:1px solid #31566a;padding:4px;}
      QToolButton {background:#0b2231;border:1px solid #23465a;padding:8px;color:#d6e7f4;}
      QLabel#stageTitle { font-weight: 700; color: #f0f6fb; qproperty-alignment: AlignCenter; }
      QLabel#sectionTitle { font-size: 13px; font-weight: 700; color: #eaf4fb; }
      QFrame[panel="true"], QFrame[stage="true"], QGroupBox {
        background: #0a1d29; border: 1px solid #24485d; border-radius: 7px;
      }
      QFrame[graphNode="true"] { background:#0a1d29; border:1px solid #24485d; border-radius:6px; }
      QFrame[placeholder="true"] { background:#081720; border:1px dashed #38505f; }
      QFrame[clickable="true"]:hover { background:#0d2736; border-color:#35caff; }
      QGroupBox { margin-top: 11px; padding-top: 10px; font-weight: 700; }
      QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #cfe6f5; }
      QPushButton { background: #0b2231; border: 1px solid #23465a; padding: 10px 18px; color: #d6e7f4; }
      QPushButton:hover { border-color: #18bfff; color: white; }
      QPushButton[active="true"] { background: #0a3148; border-bottom: 2px solid #1ac5ff; color: #59d6ff; }
      QPushButton:checked { background: #552322; border-color: #ff6767; color: #ffd4d4; }
      QPushButton[positiveToggle="true"]:checked { background:#0b3d31; border-color:#43e878; color:#c8ffda; }
      QPushButton[positiveToggle="true"]:checked:hover { background:#0e4a3b; border-color:#62f08c; color:#ffffff; }
      QTabBar#mainNavigation { background:#06131d; }
      QTabBar#mainNavigation::tab { background:#0b2231; border:1px solid #23465a; min-width:92px; padding:9px 12px; color:#d6e7f4; }
      QTabBar#mainNavigation::tab:hover { border-color:#18bfff; color:white; }
      QTabBar#mainNavigation::tab:selected { background:#0a3148; border-bottom:2px solid #1ac5ff; color:#59d6ff; }
      QTabBar#mainNavigation::tab:disabled { color:#526876; background:#091923; }
      QProgressBar { background: #041019; border: 1px solid #31566a; border-radius: 3px; text-align: center; color: white; }
      QProgressBar::chunk { background: qlineargradient(x1:0,y1:1,x2:0,y2:0, stop:0 #13bfff, stop:0.42 #18df76, stop:0.78 #d8e53b, stop:1 #ff9f32); }
      QComboBox { background:#071822; border:1px solid #31566a; padding:6px; min-height:20px; }
      QSlider::groove:horizontal { height:5px; background:#06131d; border:1px solid #274453; }
      QSlider::sub-page:horizontal { background:#32dce1; }
      QSlider::handle:horizontal { width:12px; margin:-5px 0; background:#d8f7fa; border:1px solid #32dce1; border-radius:6px; }
      QTableWidget, QTreeWidget { background: #071822; alternate-background-color: #0a202c; border: 0; gridline-color: #163849; }
      QHeaderView::section { background: #0d2634; color: #9fc2d8; border: 0; padding: 6px; font-weight: 600; }
      QTableWidget::item, QTreeWidget::item { padding: 4px; }
      QTableWidget::item:selected { background:#103e54; color:#ffffff; }
      QStatusBar { border-top: 1px solid #23495d; color: #9ebbd0; }
    )");
}
} // namespace entropy::host
