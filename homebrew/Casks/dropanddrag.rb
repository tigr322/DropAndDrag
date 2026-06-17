cask "dropanddrag" do
  version "1.0.0"
  sha256 arm:   "f5cd9d85135fa3cc59044b628a55b11e4331362fcb107a8593d41f80d1ca2ddb",
         intel: "PLACEHOLDER_X64_SHA256"

  url "https://github.com/tigr322/DropAndDrag/releases/download/v#{version}/DropAndDrag-#{version}-macOS.dmg"
  name "DropAndDrag"
  desc "Fast cross-platform drag-and-drop shelf utility"
  homepage "https://github.com/tigr322/DropAndDrag"

  depends_on macos: :ventura

  app "DropAndDrag.app"

  zap trash: [
    "~/Library/Application Support/DropAndDrag",
    "~/Library/Preferences/com.dropanddrag.app.plist",
    "~/Library/Caches/DropAndDrag",
  ]
end
