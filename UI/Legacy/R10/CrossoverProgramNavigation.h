#pragma once
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
namespace entropy::host {
// Navigation only: does not write a program parameter or any engine command.
inline void bindCrossoverProgramShortcut(QPushButton* button, QTabBar* navigation, QWidget* view) {
    QObject::connect(button, &QPushButton::clicked, view, [navigation, view] {
        auto* tabs=view->findChild<QTabWidget*>(QStringLiteral("crossoverTabs"));
        if (!tabs) return;
        int mainIndex=-1, programIndex=-1;
        for (int n=0;n<navigation->count();++n)
            if (navigation->tabText(n)==QStringLiteral("Crossover")) mainIndex=n;
        for (int n=0;n<tabs->count();++n)
            if (tabs->tabText(n)==QStringLiteral("MUSIC/CINEMA")) programIndex=n;
        if (mainIndex<0 || programIndex<0) return;
        tabs->setCurrentIndex(programIndex);
        navigation->setCurrentIndex(mainIndex);
    });
}
}
