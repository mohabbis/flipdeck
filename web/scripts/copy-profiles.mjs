import { cpSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";

const scriptDir = dirname(fileURLToPath(import.meta.url));
const sourceDir = join(scriptDir, "../../sd_card/apps_data/flipdeck/profiles");
const destDir = join(scriptDir, "../public/profiles");

try {
  cpSync(sourceDir, destDir, { recursive: true });
  console.log("✓ Profiles copied to public directory");
} catch (error) {
  console.error("Failed to copy profiles:", error);
}
