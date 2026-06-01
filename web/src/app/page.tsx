import { Profile } from "@/types/flipdeck";
import { getActionTypeColor } from "@/lib/utils";

// Profile data - copied from SD card profiles
const profiles: Profile[] = [
  {
    name: "Git",
    id: "git",
    description: "Git workflow shortcuts",
    actions: [
      { label: "Git Status", type: "text", value: "git status\n", confirm: true },
      { label: "Git Add All", type: "text", value: "git add .\n", confirm: true },
      { label: "Git Commit", type: "text", value: "git commit -m \"", confirm: true },
      { label: "Git Push", type: "text", value: "git push origin\n", confirm: true },
      { label: "Git Pull", type: "text", value: "git pull origin\n", confirm: true },
    ],
  },
  {
    name: "Node.js",
    id: "node",
    description: "Node.js development commands",
    actions: [
      { label: "Run dev server", type: "text", value: "npm run dev\n", confirm: true },
      { label: "Run tests", type: "text", value: "npm test\n", confirm: true },
      { label: "Build", type: "text", value: "npm run build\n", confirm: true },
      { label: "Install", type: "text", value: "npm install\n", confirm: true },
    ],
  },
  {
    name: "Python",
    id: "python",
    description: "Python development commands",
    actions: [
      { label: "Run Python", type: "text", value: "python\n", confirm: true },
      { label: "Run script", type: "text", value: "python ", confirm: true },
      { label: "Pip Install", type: "text", value: "pip install ", confirm: true },
      { label: "Pip List", type: "text", value: "pip list\n", confirm: true },
    ],
  },
  {
    name: "Docker",
    id: "docker",
    description: "Docker container and image commands",
    actions: [
      { label: "Docker PS", type: "text", value: "docker ps\n", confirm: true },
      { label: "Docker Images", type: "text", value: "docker images\n", confirm: true },
      { label: "Docker Compose PS", type: "text", value: "docker-compose ps\n", confirm: true },
      { label: "Docker Compose Up", type: "text", value: "docker-compose up -d\n", confirm: true },
      { label: "Docker Compose Down", type: "text", value: "docker-compose down\n", confirm: true },
    ],
  },
  {
    name: "System",
    id: "system",
    description: "System utilities",
    actions: [
      { label: "Clear", type: "text", value: "clear\n", confirm: true },
      { label: "LS", type: "text", value: "ls -la\n", confirm: true },
      { label: "PWD", type: "text", value: "pwd\n", confirm: true },
      { label: "Whoami", type: "text", value: "whoami\n", confirm: true },
    ],
  },
  {
    name: "VSCode",
    id: "vscode",
    description: "VSCode keyboard shortcuts",
    actions: [
      { label: "Command Palette", type: "key_combo", value: "CTRL+SHIFT+P", confirm: false },
      { label: "File Explorer", type: "key_combo", value: "CTRL+SHIFT+E", confirm: false },
      { label: "Search", type: "key_combo", value: "CTRL+SHIFT+F", confirm: false },
      { label: "Terminal", type: "key_combo", value: "CTRL+`", confirm: false },
      { label: "Format Document", type: "key_combo", value: "SHIFT+ALT+F", confirm: false },
    ],
  },
  {
    name: "Presentation",
    id: "presentation",
    description: "Presentation remote control",
    actions: [
      { label: "Next Slide", type: "key", value: "RIGHT", confirm: false },
      { label: "Previous Slide", type: "key", value: "LEFT", confirm: false },
      { label: "Start Slideshow", type: "key", value: "F5", confirm: false },
      { label: "Exit Presentation", type: "key", value: "ESCAPE", confirm: false },
      { label: "Blank Screen", type: "key", value: "B", confirm: false },
    ],
  },
  {
    name: "AWS CLI",
    id: "aws",
    description: "AWS command line tools",
    actions: [
      { label: "EC2 Instances", type: "text", value: "aws ec2 describe-instances\n", confirm: true },
      { label: "S3 List Buckets", type: "text", value: "aws s3 ls\n", confirm: true },
      { label: "S3 Sync", type: "text", value: "aws s3 sync . s3://\n", confirm: true },
      { label: "Lambda List", type: "text", value: "aws lambda list-functions\n", confirm: true },
      { label: "STS Get Caller", type: "text", value: "aws sts get-caller-identity\n", confirm: true },
    ],
  },
  {
    name: "Code Snippets",
    id: "snippets",
    description: "Common code templates",
    actions: [
      { label: "Console Log", type: "text", value: "console.log('DEBUG:', );\n", confirm: true },
      { label: "For Loop", type: "text", value: "for(let i = 0; i < ; i++) {\n  \n}\n", confirm: true },
      { label: "If Statement", type: "text", value: "if (condition) {\n  \n}\n", confirm: true },
      { label: "Try Catch", type: "text", value: "try {\n  \n} catch (error) {\n  console.error(error);\n}\n", confirm: true },
      { label: "Arrow Function", type: "text", value: "const func = () => {\n  \n}\n", confirm: true },
    ],
  },
];


export default function Home() {
  return (
    <div className="flex flex-col items-center justify-center min-h-screen bg-zinc-50 font-sans dark:bg-black p-4">
      <main className="flex flex-col items-center w-full max-w-5xl bg-white dark:bg-zinc-900 rounded-xl shadow-lg p-8">
        {/* Header */}
        <header className="w-full mb-8">
          <h1 className="text-4xl font-bold text-center text-zinc-900 dark:text-zinc-100 mb-2">
            FlipDeck
          </h1>
          <p className="text-lg text-center text-zinc-600 dark:text-zinc-400">
            USB Command Deck for Flipper Zero
          </p>
        </header>

        {/* Profiles Grid */}
        <div className="w-full grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
          {profiles.map((profile) => (
            <div
              key={profile.id}
              className="border border-zinc-200 dark:border-zinc-700 rounded-lg p-4 hover:shadow-md transition-shadow"
            >
              <div className="flex items-center justify-between mb-3">
                <h2 className="text-xl font-semibold text-zinc-800 dark:text-zinc-200">
                  {profile.name}
                </h2>
                <span className="text-xs px-2 py-1 bg-amber-100 text-amber-800 dark:bg-amber-900 dark:text-amber-200 rounded-full">
                  {profile.id}
                </span>
              </div>
              <p className="text-sm text-zinc-600 dark:text-zinc-400 mb-4">
                {profile.description}
              </p>
              <div className="flex justify-between items-center mb-2">
                <span className="text-xs text-zinc-500 dark:text-zinc-500">
                  {profile.actions.length} actions
                </span>
                <a
                  href={`/profiles/${profile.id}.json`}
                  download
                  className="text-xs px-2 py-1 text-amber-700 bg-amber-100 dark:bg-amber-900 dark:text-amber-200 rounded hover:bg-amber-200 dark:hover:bg-amber-800 transition-colors"
                >
                  Download
                </a>
              </div>
              <div className="space-y-2 max-h-48 overflow-y-auto">
                {profile.actions.map((action, index) => (
                  <div
                    key={index}
                    className="flex items-center justify-between p-2 bg-zinc-50 dark:bg-zinc-800 rounded"
                  >
                    <span className="text-sm font-medium text-zinc-700 dark:text-zinc-300">
                      {action.label}
                    </span>
                    <span
                      className={`text-xs px-2 py-0.5 rounded-full font-medium ${getActionTypeColor(
                        action.type
                      )}`}
                    >
                      {action.type}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          ))}
        </div>

        {/* Footer */}
        <footer className="w-full mt-8 pt-6 border-t border-zinc-200 dark:border-zinc-700">
          <div className="flex flex-col md:flex-row items-center justify-between gap-4">
            <p className="text-sm text-zinc-500 dark:text-zinc-400">
              Store profiles on your Flipper Zero SD card:{" "}
              <code className="bg-zinc-100 dark:bg-zinc-800 px-1 rounded">
                /apps_data/flipdeck/profiles/
              </code>
            </p>
            <div className="flex gap-3">
              <a
                href="/api/profiles/download"
                className="px-4 py-2 text-sm font-medium text-amber-700 bg-amber-100 dark:bg-amber-900 dark:text-amber-200 rounded-lg hover:bg-amber-200 dark:hover:bg-amber-800 transition-colors"
              >
                Download All Profiles (ZIP)
              </a>
              <a
                href="https://github.com/mohabbis/flipdeck"
                target="_blank"
                rel="noopener noreferrer"
                className="px-4 py-2 text-sm font-medium text-zinc-700 bg-zinc-100 dark:bg-zinc-800 dark:text-zinc-300 rounded-lg hover:bg-zinc-200 dark:hover:bg-zinc-700 transition-colors"
              >
                GitHub
              </a>
            </div>
          </div>
        </footer>
      </main>
    </div>
  );
}