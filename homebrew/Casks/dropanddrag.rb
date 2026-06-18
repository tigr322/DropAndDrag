cask "dropanddrag" do
  version "1.0.5"
  sha256 arm:   "d5d98127be24855005e7c41967be05ba65d763ed8756b9d05d7b5ca37bfbb1b2",
         intel: "d5d98127be24855005e7c41967be05ba65d763ed8756b9d05d7b5ca37bfbb1b2"

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

  zap trash: [
    "~/Library/Application Support/DropAndDrag",
    "~/Library/Preferences/com.dropanddrag.app.plist",
    "~/Library/Caches/DropAndDrag",
  ]
end
