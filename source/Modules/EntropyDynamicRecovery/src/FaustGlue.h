#pragma once

// Minimal C++ architecture for the generated Faust class. No Faust runtime,
// allocator, UI framework or standard library imports are linked into the DLL.
class Meta {
public:
    virtual ~Meta() = default;
    virtual void declare(const char*, const char*) = 0;
};

class UI {
public:
    virtual ~UI() = default;
    virtual void openVerticalBox(const char*) = 0;
    virtual void closeBox() = 0;
    virtual void addHorizontalSlider(const char*, double*, double, double, double, double) = 0;
};

class dsp {
public:
    virtual ~dsp() = default;
};
