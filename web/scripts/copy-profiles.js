const { cpSync } = require("fs");
const { join } = require("path");

const sourceDir = join(__dirname, "../../sd_card/apps_data/flipdeck/profiles");
const destDir = join(__dirname, "../public/profiles");

try {
  cpSync(sourceDir, destDir, { recursive: true });
  console.log("✓ Profiles copied to public directory");
} catch (error) {
  console.error("Failed to copy profiles:", error);
}