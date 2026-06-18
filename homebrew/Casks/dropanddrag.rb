cask "dropanddrag" do
  version "1.0.7"
  sha256 arm:   "b126531513733d87f572a5343bc05d93f8bb7af538d1f8a694a15816367104c0",
         intel: "b126531513733d87f572a5343bc05d93f8bb7af538d1f8a694a15816367104c0"

  url "https://github.com/tigr322/DropAndDrag/releases/download/v#{version}/DropAndDrag-#{version}-macOS.dmg"
  name "DropAndDrag"
  desc "Fast cross-platform drag-and-drop shelf utility"
  homepage "https://github.com/tigr322/DropAndDrag"

  depends_on macos: :ventura

  app "DropAndDrag.app"

  postflight do
    system_command "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister",
                   args: ["-f", "#{appdir}/DropAndDrag.app"]
    system_command "xattr", args: ["-dr", "com.apple.quarantine", "#{appdir}/DropAndDrag.app"], sudo: true
  end

  caveats <<~EOS
    For shake-to-open: System Settings → Privacy & Security → Accessibility → enable DropAndDrag
  EOS

  zap trash: ["~/Library/Application Support/DropAndDrag", "~/Library/Preferences/com.dropanddrag.app.plist", "~/Library/Caches/DropAndDrag"]
end
