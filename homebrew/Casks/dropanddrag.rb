cask "dropanddrag" do
  version "1.0.1"
  sha256 arm:   "10409b01dda56629819e9eafb09f9e4472d93f5bb0a33e4bd2889514f20fed4f",
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
