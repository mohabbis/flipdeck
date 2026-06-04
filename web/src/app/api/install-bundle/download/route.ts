import { NextResponse } from "next/server";
import JSZip from "jszip";
import { profileFiles } from "@/lib/profiles";

const APP_ROOT = "apps_data/flipdeck";

const settings = {
  confirm_before_send: true,
  auto_detect_usb: true,
  show_icons: true,
  show_descriptions: true,
  send_delay_ms: 100,
  startup_category: "",
};

const snippets: Record<string, string> = {
  "debug_log.txt": `// Debug logging snippet
console.log('[DEBUG]', 'Variable name:', variableName);
console.error('[ERROR]', errorMessage);
debugger;`,
  "go.txt": `### Main Function (Go)
func main() {
    fmt.Println("Hello, World!")
}

### Struct Definition (Go)
type  struct {
    Name string
    Age  int
}

### Goroutine
go func() {
    ()
}()

### Channel
ch := make(chan int)

### Error Handling (Go)
if err != nil {
    log.Fatal(err)
}`,
  "react_component.txt": `// React component template
import React from 'react';
import PropTypes from 'prop-types';

const ComponentName = ({ children }) => {
    return (
        <div className="component-name">
            {children}
        </div>
    );
};

ComponentName.propTypes = {
    children: PropTypes.node,
};

export default ComponentName;`,
  "typescript.txt": `### Console Log
console.log('DEBUG:', );

### Object Destructuring
const { } = ;

### Async Function
async function func() {
  try {
    const result = await ;
  } catch (error) {
    console.error(error);
  }
}

### API Route Handler
app.get('/api/', (req, res) => {
  res.json({});
});

### Type Definition (TypeScript)
interface  {
  name: string;
  value: number;
}`,
};

export async function GET() {
  const zip = new JSZip();

  zip.file(
    "README-FIRST.txt",
    [
      "FlipDeck Flipper Zero install pack",
      "",
      "1. Leave the microSD card inside your Flipper Zero.",
      "2. Plug in and unlock your Flipper Zero over USB.",
      "3. Open qFlipper on your computer and open the SD card file browser.",
      "4. Drag the apps_data folder from this ZIP onto the Flipper SD card root.",
      "5. Merge/replace the FlipDeck files when prompted.",
      "6. Launch FlipDeck from Apps on your Flipper.",
      "",
      "Manual fallback: if you are developing or qFlipper is unavailable, copy apps_data to a mounted Flipper microSD card with an external card reader, eject it, then insert it back into the Flipper.",
      "",
      "Profiles are installed to /apps_data/flipdeck/profiles/.",
      "Only use command profiles you trust on computers you own or administer.",
      "",
    ].join("\n")
  );

  for (const { fileName, profile } of profileFiles) {
    zip.file(
      `${APP_ROOT}/profiles/${fileName}`,
      `${JSON.stringify(profile, null, 2)}\n`
    );
  }

  for (const [fileName, snippet] of Object.entries(snippets)) {
    zip.file(`${APP_ROOT}/snippets/${fileName}`, `${snippet}\n`);
  }

  zip.file(`${APP_ROOT}/settings.json`, `${JSON.stringify(settings, null, 2)}\n`);

  const zipContent = await zip.generateAsync({ type: "arraybuffer" });

  return new NextResponse(Buffer.from(zipContent), {
    headers: {
      "Content-Type": "application/zip",
      "Content-Disposition": "attachment; filename=flipdeck-flipper-install-pack.zip",
    },
  });
}
