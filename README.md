# Max-Claude MCP Integration

This project provides integration between Claude AI and Max/MSP through Model Context Protocol (MCP). It consists of two main components:

1. **SQL Knowledge Base**: A database containing information about Max objects, connection patterns, and validation rules
2. **Maxpat Generator**: A tool for generating Max patch files (.maxpat) based on specifications

## Features

- **Object Information**: Lookup and search for Max/MSP objects
- **Connection Patterns**: Check valid connections between objects
- **Maxpat Generation**: Create Max patches from templates or custom specifications
- **Validation Rules**: Check for common errors and best practices

## Getting Started

### Prerequisites

- Node.js 14.x or later
- SQLite 3.x
- Claude Desktop with MCP support

### Installation

1. Clone this repository:
   ```
   git clone https://github.com/yasunoritani/manxo.git
   cd manxo
   ```

2. Install dependencies:
   ```
   npm install
   ```

### Usage

#### Start the integrated MCP server:

```
npm start
```

#### Start only the SQL knowledge base:

```
npm run start:sql
```

#### Generate a Max patch:

```
npm run start:generator
```

## MCP Tools

The following MCP tools are available:

### Knowledge Base Tools

- `getMaxObject`: Get detailed information about a Max object
- `searchMaxObjects`: Search for Max objects by name or description
- `checkConnectionPattern`: Validate connections between objects
- `searchConnectionPatterns`: Find common connection patterns

### Maxpat Generator Tools

- `generateMaxPatch`: Create a basic Max patch from a template
- `listTemplates`: Get a list of available templates
- `createAdvancedPatch`: Generate a complex patch with custom objects and connections

## License

This project is licensed under the MIT License - see the LICENSE file for details.
