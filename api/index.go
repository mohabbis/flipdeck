package main

import (
	"html/template"
	"net/http"
	"runtime"
	"time"
)

var quotes = []string{
	"Code is like humor. When you have to explain it, it's bad. - Cory House",
	"The best error message is the one that never shows up. - Thomas Fuchs",
	"Muhammad Rafiq: Making waves in the digital ocean!",
	"Hack the planet! - Flipper Zero",
}

func Handler(w http.ResponseWriter, r *http.Request) {
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
}