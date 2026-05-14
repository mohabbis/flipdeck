package main

import (
	"fmt"
	"html/template"
	"net/http"
	"os"
	"runtime"
	"time"

	"github.com/fatih/color"
	"github.com/spf13/cobra"
)

// Flipper Zero themed colors (monochrome green)
var flipperGreen = color.New(color.FgGreen)
var flipperGreenSprint = flipperGreen.SprintFunc()

// Inspirational quotes for Muhammad
var quotes = []string{
	"Code is like humor. When you have to explain it, it's bad. - Cory House",
	"The best error message is the one that never shows up. - Thomas Fuchs",
	"First, solve the problem. Then, write the code. - John Johnson",
	"Experience is the name everyone gives to their mistakes. - Oscar Wilde",
	"Simplicity is the soul of efficiency. - Austin Freeman",
	"Muhammad Rafiq: Making waves in the digital ocean!",
	"Every great developer was once a beginner. Keep building!",
	"Hack the planet! - Flipper Zero",
	"Signals everywhere, decode them all! - Flipper",
}

func main() {
	var name string
	var serve bool
	var port int

	// Check if running on Vercel (serve mode by default)
	isVercel := os.Getenv("VERCEL") == "1" || os.Getenv("AWS_LAMBDA_RUNTIME_API") != ""

	root := &cobra.Command{
		Use:   "muhome",
		Short: "Personal Dashboard CLI + Web Server",
		Long: `Muhammad's Personal Dashboard
A CLI with personality + web dashboard.
muharafiq.com | muhome.vercel.app`,
		Run: func(cmd *cobra.Command, args []string) {
			if serve || isVercel {
				startWebServer(port)
				return
			}
			runDashboard(name, args)
		},
	}

	root.Flags().StringVarP(&name, "name", "n", "", "name to greet")
	root.Flags().BoolVarP(&serve, "serve", "s", false, "start web dashboard server")
	root.Flags().IntVarP(&port, "port", "p", 8080, "port for web server (overrides PORT env)")

	root.AddCommand(newInspireCmd())
	root.AddCommand(newStatsCmd())
	root.AddCommand(newFlipperCmd())

	// Use PORT environment variable if available (for Vercel deployment)
	if port == 8080 {
		if envPort := os.Getenv("PORT"); envPort != "" {
			fmt.Sscanf(envPort, "%d", &port)
		}
	}

	if err := root.Execute(); err != nil {
		os.Exit(1)
	}
}

func runDashboard(name string, args []string) {
	who := name
	if who == "" && len(args) > 0 {
		who = args[0]
	}
	if who == "" {
		who = "Muhammad"
	}

	// Colorful banner
	cyan := color.New(color.FgCyan).SprintFunc()
	green := color.New(color.FgGreen).SprintFunc()
	yellow := color.New(color.FgYellow).SprintFunc()
	magenta := color.New(color.FgMagenta).SprintFunc()

	fmt.Println()
	fmt.Println(cyan("  ================================================"))
	fmt.Println(cyan("  |") + green("   MUHAMMAD'S TERMINAL DASHBOARD   ") + cyan("|"))
	fmt.Println(cyan("  ================================================"))
	fmt.Println()

	fmt.Printf("  %s %s%s%s! \n", yellow(">"), green("Welcome,"), magenta(" "+who), green(""))
	fmt.Println()

	// System info
	fmt.Printf("  %s %s %s\n", yellow(">"), cyan("Platform:"), runtime.GOOS+"/"+runtime.GOARCH)
	fmt.Printf("  %s %s %s\n", yellow(">"), cyan("Go Version:"), runtime.Version())
	fmt.Printf("  %s %s %d\n", yellow(">"), cyan("CPUs:"), runtime.NumCPU())
	fmt.Println()

	// Random inspirational quote
	quoteIdx := time.Now().Unix() % int64(len(quotes))
	fmt.Printf("  %s %s\n", yellow("*"), quotes[quoteIdx])
	fmt.Println()

	// Quick tips
	fmt.Printf("  %s %s\n", magenta("*"), "Use --serve to launch the web dashboard!")
	fmt.Printf("  %s %s\n", magenta("*"), "Run 'muhome inspire' for more inspiration!")
	fmt.Printf("  %s %s\n", magenta("*"), "Try 'muhome stats' for system stats!")
	fmt.Println()
}

func newInspireCmd() *cobra.Command {
	return &cobra.Command{
		Use:   "inspire",
		Short: "Get an inspirational quote",
		Run: func(cmd *cobra.Command, args []string) {
			quoteIdx := time.Now().Unix() % int64(len(quotes))
			green := color.New(color.FgGreen).SprintFunc()
			fmt.Printf("\n  %s %s\n\n", green(">"), quotes[quoteIdx])
		},
	}
}

func newStatsCmd() *cobra.Command {
	return &cobra.Command{
		Use:   "stats",
		Short: "Show system statistics",
		Run: func(cmd *cobra.Command, args []string) {
			var m runtime.MemStats
			runtime.ReadMemStats(&m)

			cyan := color.New(color.FgCyan).SprintFunc()
			green := color.New(color.FgGreen).SprintFunc()

			fmt.Println()
			fmt.Println(cyan("  ================================="))
			fmt.Println(cyan("  |") + green("   SYSTEM STATISTICS   ") + cyan("|"))
			fmt.Println(cyan("  ================================="))
			fmt.Println()
			fmt.Printf("  %s %s\n", cyan("OS:"), runtime.GOOS)
			fmt.Printf("  %s %s\n", cyan("Arch:"), runtime.GOARCH)
			fmt.Printf("  %s %d\n", cyan("CPUs:"), runtime.NumCPU())
			fmt.Printf("  %s %s\n", cyan("Go:"), runtime.Version())
			fmt.Printf("  %s %d MB\n", cyan("Alloc:"), m.Alloc/1024/1024)
			fmt.Printf("  %s %d MB\n", cyan("Sys:"), m.Sys/1024/1024)
			fmt.Println()
		},
	}
}

func newFlipperCmd() *cobra.Command {
	return &cobra.Command{
		Use:   "flipper",
		Short: "Flipper Zero themed greeting 🎮",
		Run: func(cmd *cobra.Command, args []string) {
			green := color.New(color.FgGreen).SprintFunc()
			cyan := color.New(color.FgCyan).SprintFunc()
			yellow := color.New(color.FgYellow).SprintFunc()

			fmt.Println()
			fmt.Println(cyan("  ╔═══════════════════════════════╗"))
			fmt.Println(cyan("  ║") + green("   ╔╗╔╗╔╗╔══╗╔══╗╔══╗╔═╗╔═╗   ") + cyan("║"))
			fmt.Println(cyan("  ║") + green("   ║║║║║║╚╗╔╝╚╗╔╝║╔═╣╚═╗╚═╗   ") + cyan("║"))
			fmt.Println(cyan("  ║") + green("   ╩╩╩╩╩╝ ╚╝  ╚╝ ╚══╝╚═╝╚═╝   ") + cyan("║"))
			fmt.Println(cyan("  ║") + green("   FLIPPER ZERO ACTIVATED   ") + cyan("║"))
			fmt.Println(cyan("  ╚═══════════════════════════════╝"))
			fmt.Println()
			fmt.Printf("  %s %s%s%s\n", green(">"), "Welcome to the ", yellow("digital underground"), green("!"))
			fmt.Printf("  %s %s\n", green(">"), "Signals detected: 433MHz, 868MHz, 2.4GHz")
			fmt.Printf("  %s %s\n", green(">"), "Ready to explore... 🕵️‍♂️")
			fmt.Println()
		},
	}
}

func startWebServer(port int) {
	cyan := color.New(color.FgCyan).SprintFunc()
	green := color.New(color.FgGreen).SprintFunc()

	fmt.Println()
	fmt.Println(cyan("  ================================================"))
	fmt.Println(cyan("  |") + green("   WEB DASHBOARD STARTING...   ") + cyan("|"))
	fmt.Println(cyan("  ================================================"))
	fmt.Println()

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		var m runtime.MemStats
		runtime.ReadMemStats(&m)

		data := struct {
			Title    string
			Name     string
			Time     string
			Quote    string
			Platform string
			Arch     string
			CPUs     int
			Alloc    uint64
			Sys      uint64
		}{
			Title:    "MuHome Dashboard",
			Name:     "Muhammad Rafiq",
			Time:     time.Now().Format("15:04:05"),
			Quote:    quotes[time.Now().Unix()%int64(len(quotes))],
			Platform: runtime.GOOS,
			Arch:     runtime.GOARCH,
			CPUs:     runtime.NumCPU(),
			Alloc:    m.Alloc / 1024 / 1024,
			Sys:      m.Sys / 1024 / 1024,
		}

		tmpl := `<!DOCTYPE html>
<html>
<head>
	<title>{{.Title}}</title>
	<style>
		@import url('https://fonts.googleapis.com/css2?family=Fira+Code:wght@400;600&display=swap');
		body { font-family: 'Fira Code', monospace; background: linear-gradient(135deg, #0a0a0a 0%, #1a1a1a 100%); color: #00ff00; min-height: 100vh; margin: 0; padding: 40px; }
		.container { max-width: 800px; margin: 0 auto; }
		h1 { color: #00ff00; text-shadow: 0 0 10px rgba(0,255,0,0.5); font-weight: 600; }
		.quote { background: rgba(0,255,0,0.1); padding: 20px; border-radius: 10px; margin: 20px 0; border-left: 4px solid #00ff00; }
		.stats { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin-top: 20px; }
		.stat { background: rgba(0,0,0,0.5); padding: 15px; border-radius: 8px; border: 1px solid rgba(0,255,0,0.3); }
		.label { color: #888; font-size: 0.9em; }
		.value { color: #00ff00; font-size: 1.2em; font-weight: bold; }
		.footer { margin-top: 40px; text-align: center; color: #666; }
		.glitch { animation: glitch 2s infinite; }
		@keyframes glitch { 0%, 100% { text-shadow: 0 0 5px #00ff00; } 50% { text-shadow: -2px 0 #ff00ff, 2px 0 #00ffff; } }
	</style>
</head>
<body>
	<div class="container">
		<h1 class="glitch">MuHome Dashboard</h1>
		<div class="quote">{{.Quote}}</div>
		<div class="stats">
			<div class="stat"><div class="label">Platform</div><div class="value">{{.Platform}}</div></div>
			<div class="stat"><div class="label">Architecture</div><div class="value">{{.Arch}}</div></div>
			<div class="stat"><div class="label">CPUs</div><div class="value">{{.CPUs}}</div></div>
			<div class="stat"><div class="label">Memory (Alloc)</div><div class="value">{{.Alloc}} MB</div></div>
		</div>
		<div class="footer">muharafiq.com | muhome.vercel.app | Flipper Zero Style 🐬</div>
	</div>
</body>
</html>`

		t := template.Must(template.New("dashboard").Parse(tmpl))
		t.Execute(w, data)
	})

	go func() {
		time.Sleep(500 * time.Millisecond)
		fmt.Printf("  %s %s\n", green("+"), "Dashboard available at: http://localhost:"+fmt.Sprintf("%d", port))
	}()

	if err := http.ListenAndServe(":"+fmt.Sprintf("%d", port), nil); err != nil {
		fmt.Fprintf(os.Stderr, "Server error: %v\n", err)
	}
}