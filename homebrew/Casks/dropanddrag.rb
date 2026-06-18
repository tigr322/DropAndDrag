cask "dropanddrag" do
  version "1.0.9"
  sha256 arm:   "9782a199eedb46a99b28458ae0419e63f2c55cbb5ab493d779fb76f8dca114d4", intel: "599c7253768f789f90fa49dd0ebfeb7a131a7730be7dbf53a05e776a507ea4e2"
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
