cask "dropanddrag" do
  version "1.0.3"
  sha256 arm:   "7048a72747084d3ee21e9503964a5515276d61b2848eee0347c731ffa5bd654d",
         intel: "PLACEHOLDER_X64_SHA256"
  url "https://github.com/tigr322/DropAndDrag/releases/download/v#{version}/DropAndDrag-#{version}-macOS.dmg"
  name "DropAndDrag"
  desc "Fast cross-platform drag-and-drop shelf utility"
  homepage "https://github.com/tigr322/DropAndDrag"
  depends_on macos: :ventura
  app "DropAndDrag.app"
  postflight do
    system_command "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister", args: ["-f", "#{appdir}/DropAndDrag.app"]
    system_command "xattr", args: ["-dr", "com.apple.quarantine", "#{appdir}/DropAndDrag.app"], sudo: true
  end
  caveats <<~EOS
    For shake-to-open: System Settings → Privacy & Security → Accessibility → enable DropAndDrag
  EOS
  zap trash: ["~/Library/Application Support/DropAndDrag", "~/Library/Preferences/com.dropanddrag.app.plist", "~/Library/Caches/DropAndDrag"]
end
