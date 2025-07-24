# Gemini CLI Tools

A comprehensive suite of AI-powered command-line tools for developers, featuring a full-featured Gemini API client, intelligent git commit message generation, and natural language to shell command conversion.

## 🚀 Features

### 🤖 gcli - Gemini CLI Client

- **Interactive & Non-Interactive Modes**: Full shell-like interface or single command execution
- **Key-Free Mode**: Use without API key via unofficial Google endpoint
- **File Attachments**: Support for images, documents, and piped input
- **Session Management**: Persistent conversation history
- **Streaming Responses**: Real-time output with typing indicators
- **Proxy Support**: Route requests through HTTP/HTTPS proxies
- **Cross-Platform**: Linux, macOS, and Windows support

### 📝 gcommit - AI Commit Message Generator

- **Conventional Commits**: Automatically generates properly formatted commit messages
- **Git Integration**: Analyzes staged changes to create contextual messages
- **Customizable Prompts**: Use custom prompt files for specific project needs
- **Multiple Models**: Support for different Gemini model variants
- **Verbose Mode**: Debug output to see what's being sent to AI

### 🔧 gcmd - Natural Language Command Generator

- **Shell Command Generation**: Convert natural language to precise shell commands
- **Safety Features**: Built-in dangerous command detection and confirmation
- **Clipboard Integration**: Automatically copy generated commands (default)
- **Multiple Shells**: Support for bash, zsh, fish, and other shells
- **Execution Mode**: Optionally execute commands immediately with safety checks

## 📦 Installation

### Prerequisites

- **C Compiler**: GCC or Clang
- **Dependencies**:
  - libcurl (for gcli)
  - zlib (for gcli)
  - readline (Linux/macOS) or included linenoise (Windows)

### Build from Source

```bash
# Clone the repository
git clone <repository-url>
cd gcli

# Build all tools
make

# Install system-wide (optional)
sudo make install
```

### Individual Tool Building

```bash
make build-gcli      # Build only gcli
make build-gcommit   # Build only gcommit
make build-gcmd      # Build only gcmd
```

## 🔧 Configuration

### API Key Setup (for gcli)

```bash
# Set your Gemini API key
export GEMINI_API_KEY="your-api-key-here"

# Or use the free mode (no API key required)
gcli --free "Hello, how are you?"
```

### Configuration File

Create `~/.config/gcli/config.json` (Linux/macOS) or `%APPDATA%\gcli\config.json` (Windows):

```json
{
  "api_key": "your-api-key-here",
  "model_name": "gemini-2.5-pro",
  "temperature": 0.7,
  "max_output_tokens": 8192,
  "proxy": "http://proxy.example.com:8080"
}
```

## 🎯 Quick Start

### gcli Examples

```bash
# Interactive mode
gcli

# Single question
gcli "Explain quantum computing"

# Free mode (no API key)
gcli --free "What's the weather like?"

# With file attachment
gcli "Analyze this code" --attach main.c

# Quiet mode for scripting
gcli --quiet "Generate a random password" > password.txt
```

### gcommit Examples

```bash
# Stage your changes
git add .

# Generate commit message
gcommit

# With custom model and temperature
gcommit -m gemini-1.5-flash -t 0.3

# Verbose output
gcommit -v

# Use custom prompt
gcommit -p my-commit-prompt.txt
```

### gcmd Examples

```bash
# Generate and copy command
gcmd "list all files larger than 100MB"

# Generate and execute immediately
gcmd -e "show disk usage"

# Quiet mode (no clipboard, just output)
gcmd -q "find all Python files"

# Target specific shell
gcmd -s fish "compress this directory"
```

## 🔄 Workflow Integration

### Git Workflow Enhancement

```bash
# Traditional workflow
git add .
git commit -m "fix: resolve memory leak"

# Enhanced workflow with gcommit
git add .
gcommit  # Automatically generates: "fix: resolve memory leak in worker pool"
git commit -m "$(gcommit)"

# Or create an alias
alias gacommit='git commit -m "$(gcommit)"'
gacommit  # One command for add, generate, and commit
```

### Development Automation

```bash
# Generate shell commands
gcmd "find all TODO comments in Python files"
# Output: grep -r "TODO" --include="*.py" .

# Interactive AI assistance
gcli "Help me debug this segmentation fault"

# Combine tools in scripts
#!/bin/bash
git add .
COMMIT_MSG=$(gcommit)
echo "Generated commit: $COMMIT_MSG"
git commit -m "$COMMIT_MSG"
```

## 🛠 Advanced Usage

### Custom Prompts

Create specialized prompts for different projects:

```bash
# Create a prompt for documentation commits
cat > docs-prompt.txt << 'EOF'
Generate a commit message for documentation changes.
Focus on what was documented, updated, or clarified.
Use 'docs:' prefix and be specific about the content area.
EOF

gcommit -p docs-prompt.txt
```

### Scripting Integration

```bash
# Non-interactive gcli usage
RESPONSE=$(gcli --quiet --execute "Generate a UUID")
echo "Generated UUID: $RESPONSE"

# Batch commit message generation
for branch in feature-1 feature-2; do
    git checkout $branch
    git add .
    gcommit > commit-msg-$branch.txt
done
```

### Proxy and Network Configuration

```bash
# Use with corporate proxy
gcli --proxy http://proxy.company.com:8080 "Hello"

# Free mode for restricted networks
gcli --free "What's the current time?"
```

## 🏗 Architecture

### Project Structure

```
gcli/
├── gcli.c              # Main Gemini API client (137KB binary)
├── gcommit.c           # Commit message generator (35KB binary)
├── gcmd.c              # Command generator (lightweight)
├── cJSON.c/.h          # JSON library (shared)
├── linenoise.c/.h      # Readline alternative (Windows)
├── compat.h            # Cross-platform compatibility
├── Makefile            # Unified build system
└── docs/               # Comprehensive documentation
```

### Build System

- **Unified Makefile**: Single command builds all tools
- **Cross-Platform**: Automatic Windows vs POSIX detection
- **Shared Dependencies**: Efficient library usage
- **Individual Builds**: Build tools separately if needed

## 🧪 Testing

```bash
# Test all tools
make test

# Manual testing
./gcli --help
./gcommit --help
./gcmd --help

# Test with staged changes
echo "test" > test.txt
git add test.txt
./gcommit -v
```

## 🤝 Contributing

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature-name`
3. **Make your changes** and test thoroughly
4. **Generate commit message**: `gcommit`
5. **Submit a pull request**

### Development Setup

```bash
# Build in development mode
make CFLAGS="-Wall -Wextra -g -DDEBUG"

# Clean and rebuild
make clean && make

# Install locally for testing
make install PREFIX=$HOME/.local
```

## 📋 Requirements

### System Requirements

- **Operating System**: Linux, macOS, or Windows
- **Memory**: Minimum 64MB RAM
- **Storage**: ~1MB for all binaries
- **Network**: Internet connection for API calls

### Dependencies

- **gcli**: libcurl, zlib, readline (POSIX) or linenoise (Windows)
- **gcommit**: No external dependencies (calls gcli)
- **gcmd**: No external dependencies (calls gcli)

## 🔒 Security & Privacy

### API Key Security

- Store API keys in configuration files with restricted permissions
- Use environment variables for CI/CD environments
- Free mode available for privacy-conscious users

### Command Safety

- **gcmd** includes dangerous command detection
- Confirmation prompts for potentially harmful operations
- Dry-run mode to preview commands before execution

## 📈 Performance

### Binary Sizes

- **gcli**: ~137KB (full-featured client)
- **gcommit**: ~35KB (lightweight wrapper)
- **gcmd**: ~30KB (minimal dependencies)

### Runtime Performance

- Fast startup times for all tools
- Streaming responses for real-time feedback
- Efficient memory usage and cleanup

## 🐛 Troubleshooting

### Common Issues

**API Key Problems**

```bash
# Check if API key is set
echo $GEMINI_API_KEY

# Use free mode as fallback
gcli --free "test message"
```

**Build Issues**

```bash
# Install missing dependencies (Ubuntu/Debian)
sudo apt-get install libcurl4-openssl-dev zlib1g-dev libreadline-dev

# Install missing dependencies (macOS)
brew install curl zlib readline
```

**Git Integration Issues**

```bash
# Ensure you're in a git repository
git status

# Check for staged changes
git diff --staged
```

For more detailed troubleshooting, see [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Google Gemini API** for providing the AI capabilities
- **cJSON library** for JSON parsing
- **linenoise** for cross-platform readline functionality
- **curl** for HTTP client functionality

## 🔗 Links

- **Documentation**: [docs/](docs/)
- **Examples**: [docs/USAGE_EXAMPLES.md](docs/USAGE_EXAMPLES.md)
- **Issues**: Report issues in your repository's issue tracker

---

**Made with ❤️ for developers who love efficient workflows and AI-powered tools.**
