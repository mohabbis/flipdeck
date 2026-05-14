package main

import (
	"html/template"
	"net/http"
	"runtime"
	"time"
)

var quotes = []string{
	"Muhammad Rafiq: Making waves in the digital ocean!",
	"Hack the planet! - Flipper Zero",
	"Code is like humor. When you have to explain it, it's bad.",
}

// Handler is the entry point for Vercel serverless functions
func Handler(w http.ResponseWriter, r *http.Request) {
	var m runtime.MemStats
	runtime.ReadMemStats(&m)

	tmpl := `<<!DOCTYPE html><html><head><title>MuHome Dashboard</title><style>body{font-family:monospace;background:#0a0a0a;color:#00ff00;padding:40px}.container{max-width:600px;margin:0 auto}h1{color:#00ff00}.quote{background:rgba(0,255,0,0.1);padding:20px;border-radius:10px;margin:20px 0;border-left:4px solid #00ff00}.stats{display:grid;grid-template-columns:repeat(2,1fr);gap:15px}.stat{background:rgba(0,0,0,0.5);padding:15px;border-radius:8px;border:1px solid rgba(0,255,0,0.3)}.label{color:#888;font-size:0.9em}.value{color:#00ff00;font-size:1.2em;font-weight:bold}.footer{margin-top:40px;text-align:center;color:#666}</style></head><body><div class="container"><h1>MuHome Dashboard</h1><div class="quote">{{.Quote}}</div><div class="stats"><div class="stat"><div class="label">Platform</div><div class="value">{{.Platform}}</div></div><div class="stat"><div class="label">CPUs</div><div class="value">{{.CPUs}}</div></div><div class="stat"><div class="label">Memory</div><div class="value">{{.Alloc}} MB</div></div></div><div class="footer">muharafiq.com | Flipper Zero Style 🐬</div></div></body></html>`

	t := template.Must(template.New("dashboard").Parse(tmpl))
	t.Execute(w, map[string]interface{}{
		"Quote":    quotes[time.Now().Unix()%int64(len(quotes))],
		"Platform": runtime.GOOS + "/" + runtime.GOARCH,
		"CPUs":     runtime.NumCPU(),
		"Alloc":    m.Alloc / 1024 / 1024,
	})
}

func main() {
	http.HandleFunc("/", Handler)
	port := "8080"
	if p := getenv("PORT"); p != "" {
		port = p
	}
	http.ListenAndServe(":"+port, nil)
}

func getenv(k string) string { return "" }