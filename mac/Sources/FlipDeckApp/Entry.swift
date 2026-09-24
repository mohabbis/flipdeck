#if !os(macOS)
@main enum FlipDeckMain { static func main() { print("The FlipDeck app requires macOS. Use flipdeck-headless on other platforms.") } }
#endif
