# moswagger

A cleaner OpenAPI playground for people who need to understand, share, and improve API specs without fighting a wall of YAML.

moswagger is being shaped as a lightweight developer tool for reviewing Swagger/OpenAPI files, turning endpoints into human-readable summaries, and giving API projects a sharper documentation workflow.

## Why this exists

Swagger tooling is powerful, but most workflows still feel split across generators, validators, static docs, mock servers, and screenshots pasted into chats. moswagger should become the thin control panel that makes an API spec easier to inspect, explain, and ship.

## Product direction

The project should focus on five jobs:

1. **Preview** an OpenAPI spec quickly.
2. **Explain** endpoints in plain English.
3. **Validate** specs before they break downstream tools.
4. **Share** API documentation with non-engineers.
5. **Generate** useful starter assets such as example requests, mock responses, and README sections.

## Near-term feature set

- OpenAPI/Swagger file upload or paste-in editor
- Endpoint list grouped by tag and method
- Request/response schema viewer
- Plain-English endpoint summaries
- Example curl and fetch snippets
- Spec validation with actionable errors
- Exportable markdown documentation
- Example API spec for demos and tests

## Differentiation

Most Swagger tools are either visually stale or overly technical. moswagger should feel like a small design-forward API studio: readable, fast, useful, and polished enough to include in a portfolio.

## Example workflow

```bash
# validate the sample spec
npx @redocly/cli lint examples/openapi.yaml
```

```bash
# generate static docs from the sample spec
npx @redocly/cli build-docs examples/openapi.yaml --output docs/api-reference.html
```

## Suggested architecture

```text
moswagger/
├── examples/              # Demo OpenAPI specs
├── docs/                  # Product roadmap and generated docs
├── src/                   # App or package source code
├── tests/                 # Spec parsing and validation tests
└── README.md              # Product positioning and quickstart
```

## Portfolio framing

**moswagger** is a design-forward OpenAPI workflow tool that turns raw API specs into readable documentation, validation feedback, and shareable endpoint summaries.

## Roadmap

See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md).
