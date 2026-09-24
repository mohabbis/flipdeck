#if os(macOS)
import AppKit
import SwiftUI

@main
struct FlipDeckMain: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup("FlipDeck") {
            RootView()
                .environmentObject(model)
                .frame(minWidth: 880, minHeight: 560)
                .onAppear { appDelegate.model = model }
        }
        .windowToolbarStyle(.unifiedCompact)
        .commands {
            CommandGroup(after: .toolbar) {
                Button("Refresh") { model.refresh() }
                    .keyboardShortcut("r")
            }
            CommandMenu("Go") {
                ForEach(Array(SidebarItem.allCases.enumerated()), id: \.element) { index, section in
                    Button(section.title) { model.section = section }
                        .keyboardShortcut(KeyEquivalent(Character(String(index + 1))), modifiers: .command)
                }
            }
        }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    weak var model: AppModel?

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Needed when launched as a bare SwiftPM executable; harmless in a bundle.
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        guard let model else { return .terminateNow }
        // Say goodbye to the Flipper and flush the activity log first.
        Task { @MainActor in
            await model.shutdown()
            sender.reply(toApplicationShouldTerminate: true)
        }
        return .terminateLater
    }
}
#else
@main
enum FlipDeckMain {
    static func main() {
        print("The FlipDeck app requires macOS. Use flipdeck-headless on other platforms.")
    }
}
#endif
