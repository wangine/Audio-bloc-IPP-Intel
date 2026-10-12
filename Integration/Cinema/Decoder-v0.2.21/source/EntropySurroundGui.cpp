#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>
#include <windows.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace
{
constexpr auto stopEventName = L"Local\\EntropySurroundDecoder.Stop";
constexpr auto runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto runValue = L"EntropySurroundDecoder";

bool isStartupEnabled()
{
    wchar_t value[32768]{};
    DWORD bytes = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, runKey, runValue, RRF_RT_REG_SZ,
                        nullptr, value, &bytes) == ERROR_SUCCESS;
}

bool setStartupEnabled(bool enabled)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, runKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    LSTATUS result = ERROR_SUCCESS;
    if (enabled)
    {
        const auto executableText = juce::File::getSpecialLocation(
            juce::File::currentExecutableFile).getFullPathName();
        const std::wstring executable(executableText.toWideCharPointer());
        const std::wstring command = L"\"" + executable +
                                     L"\" --autostart-dddts";
        result = RegSetValueExW(key, runValue, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    }
    else
    {
        result = RegDeleteValueW(key, runValue);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

class IndicatorCard final : public juce::Component
{
public:
    explicit IndicatorCard(juce::String heading) : heading_(std::move(heading)) {}

    void setValue(juce::String value, juce::String detail, juce::Colour accent)
    {
        if (value_ == value && detail_ == detail && accent_ == accent) return;
        value_ = std::move(value);
        detail_ = std::move(detail);
        accent_ = accent;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xff17202d));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(accent_.withAlpha(0.72f));
        g.drawRoundedRectangle(bounds, 8.0f, 1.3f);

        g.setColour(accent_.withAlpha(0.18f));
        g.fillEllipse(15.0f, 17.0f, 22.0f, 22.0f);
        g.setColour(accent_);
        g.fillEllipse(20.0f, 22.0f, 12.0f, 12.0f);

        g.setColour(juce::Colour(0xff91a0b8));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(heading_, 48, 10, getWidth() - 60, 20,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xffeef4fb));
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.drawText(value_, 48, 30, getWidth() - 60, 27,
                   juce::Justification::centredLeft);

        g.setColour(accent_.interpolatedWith(
            juce::Colour(0xffc6d2e3), 0.55f));
        g.setFont(juce::FontOptions(11.5f));
        g.drawText(detail_, 48, 57, getWidth() - 60, 18,
                   juce::Justification::centredLeft);
    }

private:
    juce::String heading_, value_ {"WAITING"}, detail_;
    juce::Colour accent_ {0xff6f7d90};
};

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    explicit MainComponent(bool autoStart)
        : autoStartPending(autoStart),
          engineCard("ENGINE"), signalCard("INPUT SIGNAL"),
          codecCard("CODEC / LOCK"), outputCard("OUTPUT")
    {
        title.setText("ENTROPY SURROUND DECODER  v" ENTROPY_SURROUND_VERSION,
                      juce::dontSendNotification);
        title.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        title.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(title);

        subtitle.setText("UR23 optical  |  IEC61937  |  Entropy Matrix  |  AUTO RECOVERY",
                         juce::dontSendNotification);
        subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff91a0b8));
        addAndMakeVisible(subtitle);

        state.setText("STOPPED", juce::dontSendNotification);
        state.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        state.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(state);

        configureButton(encoded, "FULL AUTO", [this] { startDecoder(Mode::encoded); });
        configureButton(stereo51, "STEREO  >  5.1", [this] { startDecoder(Mode::stereo51); });
        configureButton(stereo71, "STEREO  >  7.1", [this] { startDecoder(Mode::stereo71); });
        configureButton(stop, "STOP", [this] { requestStop(); });
        stop.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff782d37));

        startup.setButtonText("START WITH WINDOWS - FULL AUTO HIDDEN IN TRAY");
        startup.setToggleState(isStartupEnabled(), juce::dontSendNotification);
        startup.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffc6d2e3));
        startup.onClick = [this]
        {
            const bool requested = startup.getToggleState();
            if (!setStartupEnabled(requested))
            {
                startup.setToggleState(!requested, juce::dontSendNotification);
                status.setText("ERROR: Windows startup setting could not be changed.",
                               juce::dontSendNotification);
            }
            else
            {
                status.setText(requested
                    ? "Windows logon: FULL AUTO will start hidden in tray."
                    : "Windows logon autostart disabled.", juce::dontSendNotification);
            }
        };
        addAndMakeVisible(startup);

        channels.setText("CH01  FL     CH02  FR     CH03  C      CH04  LFE\n"
                         "CH05  SL     CH06  SR     CH07  BL     CH08  BR",
                         juce::dontSendNotification);
        channels.setFont(juce::FontOptions(15.0f));
        channels.setJustificationType(juce::Justification::centred);
        channels.setColour(juce::Label::backgroundColourId, juce::Colour(0xff17202d));
        addAndMakeVisible(channels);

        status.setText("Ready. Entropy Matrix shared memory: 8 channels.", juce::dontSendNotification);
        status.setColour(juce::Label::textColourId, juce::Colour(0xffc6d2e3));
        status.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(status);

        addAndMakeVisible(engineCard);
        addAndMakeVisible(signalCard);
        addAndMakeVisible(codecCard);
        addAndMakeVisible(outputCard);
        engineCard.setValue("STOPPED", "Decoder engine idle", neutralColour);
        signalCard.setValue("NO SIGNAL", "Waiting for UR23", waitingColour);
        codecCard.setValue("WAITING", "No stream lock", neutralColour);
        outputCard.setValue("MUTED", "Digital silence", waitingColour);

        setSize(800, 540);
        updateControls();
        startTimer(100);
    }

    ~MainComponent() override
    {
        requestStop();
        if (process != nullptr && process->isRunning())
        {
            for (int i = 0; i < 20 && process->isRunning(); ++i) juce::Thread::sleep(50);
            if (process->isRunning()) process->kill();
        }
        if (outputThread.joinable()) outputThread.join();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff101722));
        g.setColour(juce::Colour(0xff263448));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 10.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(26);
        auto header = area.removeFromTop(62);
        state.setBounds(header.removeFromRight(135).reduced(4, 10));
        title.setBounds(header.removeFromTop(36));
        subtitle.setBounds(header);
        area.removeFromTop(14);
        auto buttons = area.removeFromTop(48);
        const int gap = 9;
        const int width = (buttons.getWidth() - 3 * gap) / 4;
        encoded.setBounds(buttons.removeFromLeft(width)); buttons.removeFromLeft(gap);
        stereo51.setBounds(buttons.removeFromLeft(width)); buttons.removeFromLeft(gap);
        stereo71.setBounds(buttons.removeFromLeft(width)); buttons.removeFromLeft(gap);
        stop.setBounds(buttons);
        area.removeFromTop(8);
        startup.setBounds(area.removeFromTop(30));
        area.removeFromTop(8);
        channels.setBounds(area.removeFromTop(68));
        area.removeFromTop(10);
        status.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
        auto firstRow = area.removeFromTop((area.getHeight() - 8) / 2);
        auto secondRow = area;
        const int cardGap = 10;
        engineCard.setBounds(firstRow.removeFromLeft(
            (firstRow.getWidth() - cardGap) / 2));
        firstRow.removeFromLeft(cardGap);
        signalCard.setBounds(firstRow);
        codecCard.setBounds(secondRow.removeFromLeft(
            (secondRow.getWidth() - cardGap) / 2));
        secondRow.removeFromLeft(cardGap);
        outputCard.setBounds(secondRow);
    }

private:
    template <typename Callback>
    void configureButton(juce::TextButton& button, const juce::String& text, Callback callback)
    {
        button.setButtonText(text);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff244c70));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff277a55));
        button.onClick = std::move(callback);
        addAndMakeVisible(button);
    }

    enum class Mode { stopped, encoded, stereo51, stereo71 };

    inline static const juce::Colour runningColour {0xff31c77b};
    inline static const juce::Colour waitingColour {0xffe3a83b};
    inline static const juce::Colour errorColour {0xffe05b68};
    inline static const juce::Colour neutralColour {0xff6f7d90};

    void startDecoder(Mode mode)
    {
        if (process != nullptr && process->isRunning()) return;
        if (outputThread.joinable()) outputThread.join();
        ur23Busy = false;
        runFailed = false;
        engineReady = false;
        const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                    .getSiblingFile("EntropySurroundDecoder.exe");
        if (!executable.existsAsFile())
        {
            status.setText("ERROR: EntropySurroundDecoder.exe is missing.", juce::dontSendNotification);
            return;
        }
        juce::StringArray command;
        command.add(executable.getFullPathName());
        if (mode == Mode::stereo51 || mode == Mode::stereo71)
            command.add("--stereo");
        if (mode == Mode::stereo71)
            command.add("--7.1");
        process = std::make_unique<juce::ChildProcess>();
        lineBuffer.clear();
        if (!process->start(command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            process.reset();
            status.setText("ERROR: decoder engine could not be started.", juce::dontSendNotification);
            updateControls();
            return;
        }
        activeMode = mode;
        stopping = false;
        startRequested = juce::Time::getMillisecondCounter();
        auto* child = process.get();
        outputThread = std::thread([this, child]
        {
            // JUCE's Windows ChildProcess::read() waits until the complete
            // requested byte count has accumulated, not merely until some
            // output is available. Requesting 8191 bytes trapped heartbeat
            // lines in the pipe for tens of seconds and caused a false startup
            // timeout while audio was already running. Small chunks deliver
            // every diagnostic line promptly without touching the UI thread.
            std::array<char, 64> bytes{};
            for (;;)
            {
                const int received = child->readProcessOutput(
                    bytes.data(), 32);
                if (received > 0)
                {
                    const auto text = juce::String::fromUTF8(bytes.data(), received);
                    std::lock_guard lock(outputMutex);
                    pendingOutput += text;
                    continue;
                }
                if (!child->isRunning()) break;
                juce::Thread::sleep(10);
            }
        });
        status.setText("Starting UR23 capture and Entropy 8-channel transport...", juce::dontSendNotification);
        engineCard.setValue("STARTING", "Opening UR23 exclusive", waitingColour);
        signalCard.setValue("DETECTING", "48 kHz / 16-bit input", waitingColour);
        codecCard.setValue("WAITING", "Automatic detection", waitingColour);
        outputCard.setValue("MUTED", "Safe during lock", waitingColour);
        updateControls();
    }

    juce::String selectedLayout() const
    {
        return activeMode == Mode::stereo71 ? "7.1" : "5.1";
    }

    void processDiagnosticLine(const juce::String& rawLine)
    {
        const auto line = rawLine.trim();
        if (line.isEmpty()) return;

        if (line.startsWith("Codec="))
        {
            // Ignore diagnostics already buffered in the pipe after a
            // requested stop or a terminal startup error. Otherwise a late
            // PCM line can paint green cards below an ERROR state.
            if (stopping || runFailed) return;
            engineReady = true;
            auto recoveryDetail = juce::String("Auto recovery active");
            const auto recoveryField = line.fromFirstOccurrenceOf(
                "recoveries=", false, false).upToFirstOccurrenceOf(" |", false, false);
            if (recoveryField.isNotEmpty())
                recoveryDetail = "Auto recoveries: " + recoveryField;
            engineCard.setValue("RUNNING", recoveryDetail, runningColour);
            if (line.containsIgnoreCase("NO SIGNAL"))
            {
                signalCard.setValue("NO SIGNAL", "Carrier absent", waitingColour);
                codecCard.setValue("WAITING", "No stream lock", waitingColour);
                outputCard.setValue("MUTED", "Digital silence", waitingColour);
            }
            else if (line.containsIgnoreCase("PCM Stereo"))
            {
                signalCard.setValue("PCM 2CH", "48 kHz / 16-bit", runningColour);
                codecCard.setValue("PCM STEREO", "Matrix upmix active", runningColour);
                outputCard.setValue(selectedLayout(), "8-channel Entropy bus", runningColour);
            }
            else if (line.containsIgnoreCase("Dolby Digital"))
            {
                signalCard.setValue("IEC61937", "Carrier locked", runningColour);
                if (activeMode == Mode::encoded)
                {
                    codecCard.setValue("DOLBY DIGITAL", "AC-3 decoded", runningColour);
                    outputCard.setValue("5.1", "Native decoded channels", runningColour);
                }
                else
                {
                    codecCard.setValue("DOLBY DIGITAL", "Use FULL AUTO", waitingColour);
                    outputCard.setValue("MUTED", "Encoded carrier protected", waitingColour);
                }
            }
            else if (line.containsIgnoreCase("DTS"))
            {
                signalCard.setValue("IEC61937", "Carrier locked", runningColour);
                if (activeMode == Mode::encoded)
                {
                    codecCard.setValue("DTS", "Core stream decoded", runningColour);
                    outputCard.setValue("5.1", "Native decoded channels", runningColour);
                }
                else
                {
                    codecCard.setValue("DTS", "Use FULL AUTO", waitingColour);
                    outputCard.setValue("MUTED", "Encoded carrier protected", waitingColour);
                }
            }
            status.setText(line.upToFirstOccurrenceOf(" | IEC bursts=", false, false),
                           juce::dontSendNotification);
            return;
        }

        if (line.startsWithIgnoreCase("ERROR:"))
        {
            runFailed = true;
            engineCard.setValue("ERROR", "Decoder stopped", errorColour);
            outputCard.setValue("MUTED", "Fail-safe active", errorColour);
            if (line.containsIgnoreCase("0x8889000A"))
            {
                ur23Busy = true;
                signalCard.setValue("UR23 BUSY", "Used by another app", errorColour);
                codecCard.setValue("NO LOCK", "Release capture input", errorColour);
                status.setText(
                    "UR23 is busy. Close the app using its input and try again.",
                    juce::dontSendNotification);
            }
            else if (line.containsIgnoreCase("0x80070057"))
            {
                signalCard.setValue("DRIVER ERROR", "UR23 rejected 48k/16", errorColour);
                codecCard.setValue("NO LOCK", "Capture unavailable", errorColour);
                status.setText(
                    "UR23 driver rejected capture (0x80070057). Reconnect or change driver.",
                    juce::dontSendNotification);
            }
            else
            {
                if (line.containsIgnoreCase("DEVICE_INVALIDATED") ||
                    line.containsIgnoreCase("disconnected"))
                {
                    signalCard.setValue("DISCONNECTED", "Reconnect UR23", errorColour);
                    codecCard.setValue("NO LOCK", "Device unavailable", errorColour);
                    status.setText("UR23 disconnected. Output is safely muted.",
                                   juce::dontSendNotification);
                }
                else
                    status.setText(line, juce::dontSendNotification);
            }
            return;
        }

        if (line.startsWithIgnoreCase("Input :"))
            status.setText(line, juce::dontSendNotification);
    }

    void consumeProcessOutput()
    {
        juce::String text;
        {
            std::lock_guard lock(outputMutex);
            std::swap(text, pendingOutput);
        }
        if (text.isEmpty()) return;
        lineBuffer += text;
        for (;;)
        {
            const int newline = lineBuffer.indexOfChar('\n');
            if (newline < 0) break;
            processDiagnosticLine(lineBuffer.substring(0, newline));
            lineBuffer = lineBuffer.substring(newline + 1);
        }
    }

    void requestStop()
    {
        if (process == nullptr || !process->isRunning()) return;
        if (HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, stopEventName))
        {
            SetEvent(event);
            CloseHandle(event);
        }
        stopping = true;
        stopRequested = juce::Time::getMillisecondCounter();
        status.setText("Stopping safely...", juce::dontSendNotification);
        updateControls();
    }

    void timerCallback() override
    {
        if (autoStartPending)
        {
            autoStartPending = false;
            startDecoder(Mode::encoded);
        }
        if (process == nullptr) return;
        consumeProcessOutput();
        if (!stopping && process->isRunning() && !engineReady &&
            juce::Time::getMillisecondCounter() - startRequested > 30000)
        {
            runFailed = true;
            requestStop();
            engineCard.setValue("TIMEOUT", "UR23 did not open", errorColour);
            signalCard.setValue("UNAVAILABLE", "Reset or reconnect UR23", errorColour);
            codecCard.setValue("NO LOCK", "Capture initialization failed", errorColour);
            outputCard.setValue("MUTED", "Fail-safe active", errorColour);
            status.setText("UR23 capture timed out. Reconnect the interface or restart Windows.",
                           juce::dontSendNotification);
        }
        if (stopping && process->isRunning() &&
            juce::Time::getMillisecondCounter() - stopRequested > 3000)
            process->kill();
        if (!process->isRunning() && wasRunning)
        {
            if (outputThread.joinable()) outputThread.join();
            consumeProcessOutput();
            if (lineBuffer.isNotEmpty())
            {
                processDiagnosticLine(lineBuffer);
                lineBuffer.clear();
            }
            state.setText(runFailed ? "ERROR" : "STOPPED", juce::dontSendNotification);
            if (!runFailed)
                status.setText("Decoder stopped.", juce::dontSendNotification);
            stopping = false;
            activeMode = Mode::stopped;
            if (!runFailed)
            {
                engineCard.setValue("STOPPED", "Decoder engine idle", neutralColour);
                signalCard.setValue("NO SIGNAL", "Waiting for start", neutralColour);
                codecCard.setValue("WAITING", "No stream lock", neutralColour);
                outputCard.setValue("MUTED", "Digital silence", waitingColour);
            }
            updateControls();
        }
        wasRunning = process->isRunning();
    }

    void updateControls()
    {
        const bool active = process != nullptr && process->isRunning();
        encoded.setEnabled(!active);
        stereo51.setEnabled(!active);
        stereo71.setEnabled(!active);
        stop.setEnabled(active && !stopping);
        state.setText(active ? (stopping ? "STOPPING" : "RUNNING")
                             : (runFailed ? "ERROR" : "STOPPED"),
                      juce::dontSendNotification);
        state.setColour(juce::Label::textColourId,
                        active ? juce::Colour(0xff58d68d)
                               : (runFailed ? errorColour : juce::Colour(0xff91a0b8)));
        wasRunning = active;
    }

    juce::Label title, subtitle, state, channels, status;
    IndicatorCard engineCard, signalCard, codecCard, outputCard;
    juce::TextButton encoded, stereo51, stereo71, stop;
    juce::ToggleButton startup;
    std::unique_ptr<juce::ChildProcess> process;
    std::thread outputThread;
    std::mutex outputMutex;
    juce::String pendingOutput;
    juce::String lineBuffer;
    bool wasRunning = false;
    bool stopping = false;
    bool autoStartPending = false;
    bool ur23Busy = false;
    bool runFailed = false;
    bool engineReady = false;
    Mode activeMode = Mode::stopped;
    std::uint32_t stopRequested = 0;
    std::uint32_t startRequested = 0;
};

class EntropySurroundApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "Entropy Surround Decoder v" ENTROPY_SURROUND_VERSION;
    }
    const juce::String getApplicationVersion() override { return ENTROPY_SURROUND_VERSION; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override
    {
        const bool autoStart = commandLine.contains("--autostart-dddts");
        window = std::make_unique<MainWindow>(
            getApplicationName(), autoStart);
        const auto icon = juce::ImageFileFormat::loadFrom(
            BinaryData::entropy_icon_png, BinaryData::entropy_icon_pngSize);
        tray = std::make_unique<TrayIcon>(icon,
            [this] { if (window != nullptr) window->showFromTray(); },
            [this] { quit(); });
    }
    void shutdown() override { tray.reset(); window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class TrayIcon final : public juce::SystemTrayIconComponent
    {
    public:
        TrayIcon(const juce::Image& icon, std::function<void()> show,
                 std::function<void()> exit)
            : show_(std::move(show)), exit_(std::move(exit))
        {
            setIconImage(icon, icon);
            setIconTooltip("Entropy Surround Decoder v" ENTROPY_SURROUND_VERSION);
        }

        void mouseDoubleClick(const juce::MouseEvent&) override { show_(); }
        void mouseDown(const juce::MouseEvent& event) override
        {
            if (!event.mods.isPopupMenu())
            {
                show_();
                return;
            }
            juce::PopupMenu menu;
            menu.addItem(1, "Show Entropy Surround Decoder");
            menu.addSeparator();
            menu.addItem(2, "Exit");
            menu.showMenuAsync(juce::PopupMenu::Options(), [this](int selected)
            {
                if (selected == 1) show_();
                else if (selected == 2) exit_();
            });
        }

    private:
        std::function<void()> show_, exit_;
    };

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, bool autoStart)
            : DocumentWindow(name, juce::Colour(0xff101722), closeButton)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(autoStart), true);
            setResizable(false, false);
            centreWithSize(getWidth(), getHeight());
            setVisible(!autoStart);
        }
        void closeButtonPressed() override { setVisible(false); }
        void showFromTray()
        {
            setVisible(true);
            toFront(true);
        }
    };
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<TrayIcon> tray;
};
}

START_JUCE_APPLICATION(EntropySurroundApplication)
