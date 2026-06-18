cask "dropanddrag" do
  version "1.0.11"
  sha256 arm: "b221265fdde575995db2ec22950ac6e0e3dd02115df390192731a690b6ddb1a2", intel: "b221265fdde575995db2ec22950ac6e0e3dd02115df390192731a690b6ddb1a2"
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
  caveats "For shake-to-open: System Settings → Privacy & Security → Accessibility → enable DropAndDrag"
  zap trash: ["~/Library/Application Support/DropAndDrag", "~/Library/Preferences/com.dropanddrag.app.plist", "~/Library/Caches/DropAndDrag"]
end
