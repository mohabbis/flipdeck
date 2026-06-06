export const DEFAULT_SETTINGS = {
  confirm_before_send: true,
  auto_detect_usb: true,
  show_icons: true,
  show_descriptions: true,
  send_delay_ms: 100,
  startup_category: "",
};

export const SNIPPETS: Record<string, string> = {
  "debug_log.txt": `// Debug logging snippet
console.log('[DEBUG]', 'value:', variableName);
console.error('[ERROR]', errorMessage);
debugger;
console.trace('[TRACE]', 'Stack trace above');`,
  "go.txt": `### Main Function (Go)
func main() {
    fmt.Println("Hello, World!")
}

### Struct Definition (Go)
type User struct {
    Name string
    Age  int
}

### Goroutine
go func() {
    fmt.Println("Running in background")
}()

### Channel
ch := make(chan int)

### Error Handling (Go)
if err != nil {
    log.Fatal(err)
}`,
  "react_component.txt": `// React component template
import { FC, ReactNode } from 'react';

interface ComponentNameProps {
  children?: ReactNode;
}

export const ComponentName: FC<ComponentNameProps> = ({ children }) => {
    return (
        <div className="component-name">
            {children}
        </div>
    );
};

export default ComponentName;`,
  "typescript.txt": `### Console Log
console.log('DEBUG:', value);

### Object Destructuring
const { name, value } = obj;

### Async Function
async function fetchData() {
  try {
    const result = await fetch('/api/data');
  } catch (error) {
    console.error(error);
  }
}

### API Route Handler
app.get('/api/users', (req, res) => {
  res.json({ users: [] });
});

### Type Definition (TypeScript)
interface User {
  name: string;
  value: number;
}`,
};
