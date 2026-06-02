const fs = require("fs");
const path = require("path");

const outDir = path.join(__dirname, "..", ".railway-build");
fs.mkdirSync(outDir, { recursive: true });
fs.writeFileSync(
  path.join(outDir, "packs.json"),
  JSON.stringify({ generatedAt: new Date().toISOString(), status: "runtime-pack-api-enabled" }, null, 2)
);

console.log("Prepared FlipDeck pack manifest.");
