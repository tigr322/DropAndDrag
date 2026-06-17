cask "dropanddrag" do
  version "1.0.0"
  sha256 arm:   "PLACEHOLDER_ARM64_SHA256",
         intel: "PLACEHOLDER_X64_SHA256"

  url "https://github.com/anomalyco/DropAndDrag/releases/download/v#{version}/DropAndDrag-#{version}.dmg"
  name "DropAndDrag"
  desc "Fast cross-platform drag-and-drop shelf utility"
  homepage "https://github.com/anomalyco/DropAndDrag"

  depends_on macos: ">= :ventura"

  app "DropAndDrag.app"

  zap trash: [
    "~/Library/Application Support/DropAndDrag",
    "~/Library/Preferences/com.dropanddrag.app.plist",
    "~/Library/Caches/DropAndDrag",
  ]
end
