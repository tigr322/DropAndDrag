cask "dropanddrag" do
  version "1.0.4"
  sha256 arm:   "51df1b519ce98a95ba5b07c5b3a05ca26fc5d3a953c4ac13109ea5c0a0991cb2",
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
