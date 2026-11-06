# Sorena Emacs

**Author: Sorena Arshiyan**

A tiny, very fast terminal text editor with real **Emacs key bindings** and the
everyday comforts of **Visual Studio Code**: automatic indentation, auto-closing
brackets and quotes, unlimited undo/redo, line duplication, moving lines,
comment toggling, selection highlighting, line numbers, bracket matching and
instant TAB completion of file names, buffers and commands.

It starts in milliseconds, opens a 1 000 000-line / 66 MB file in about 0.15 s
and keeps every keystroke instant even in huge files.

---

## Table of contents

1. [Building and installing](#building-and-installing)
2. [Starting the editor](#starting-the-editor)
3. [How to read the key names](#how-to-read-the-key-names)
4. [Every key binding](#every-key-binding)
   - [Help](#help)
   - [Files](#files)
   - [Buffers](#buffers)
   - [Windows](#windows)
   - [Cursor movement](#cursor-movement)
   - [Scrolling and screen](#scrolling-and-screen)
   - [Inserting and deleting](#inserting-and-deleting)
   - [Undo and redo](#undo-and-redo)
   - [Kill (cut), copy and yank (paste)](#kill-cut-copy-and-yank-paste)
   - [Selection (the region)](#selection-the-region)
   - [Line operations (VS Code style)](#line-operations-vs-code-style)
   - [Indentation](#indentation)
   - [Comments](#comments)
   - [Case changes and transposing](#case-changes-and-transposing)
   - [Whitespace](#whitespace)
   - [Completion](#completion)
   - [Searching and replacing](#searching-and-replacing)
   - [Keyboard macros](#keyboard-macros)
   - [Toggles and settings](#toggles-and-settings)
   - [Shell and miscellaneous](#shell-and-miscellaneous)
   - [Keys inside the minibuffer (prompts)](#keys-inside-the-minibuffer-prompts)
   - [Keys inside incremental search](#keys-inside-incremental-search)
   - [Keys inside query-replace](#keys-inside-query-replace)
5. [Automatic behaviour (VS Code features)](#automatic-behaviour-vs-code-features)
6. [The mode line](#the-mode-line)
7. [Commands without a key (M-x)](#commands-without-a-key-m-x)
8. [Customising (~/.emacsrc)](#customising-emacsrc)
9. [Terminal notes and troubleshooting](#terminal-notes-and-troubleshooting)

---

## Building and installing

You need a C compiler (gcc or clang), `make` and the ncurses development files.

```sh
# Debian / Ubuntu
sudo apt install build-essential libncurses-dev pkg-config
# Fedora
sudo dnf install gcc make ncurses-devel pkgconf
# Arch
sudo pacman -S base-devel ncurses
# macOS (Xcode command line tools already include ncurses)
xcode-select --install
```

Build and install:

```sh
make
sudo make install
```

This installs the editor as **`/usr/local/bin/emacs`**, so typing `emacs` in a
terminal starts it. `/usr/local/bin` comes before `/usr/bin` in the default
`PATH`, so it wins even if GNU Emacs is also installed. If your shell still
finds another `emacs`, run `hash -r` or open a new terminal.

Other options:

| Command | What it does |
|---|---|
| `make install PREFIX=$HOME/.local` | Install for your user only (no sudo) into `~/.local/bin/emacs` |
| `make NATIVE=1` | Optimise for your exact CPU (`-march=native`) |
| `make HUNSPELL=1` | Build with the optional spell-checking mode (needs libhunspell) |
| `make uninstall` | Remove the installed `emacs` |
| `make clean` | Remove build files |

The build uses `-O2` and link-time optimisation automatically when your
compiler supports it.

---

## Starting the editor

```sh
emacs                 # empty *scratch* buffer
emacs file.c          # open a file
emacs a.c b.c c.h     # open several files (C-x b or C-x <right> to switch)
emacs +120 file.c     # open at line 120
emacs -g120 file.c    # same
emacs -sfoo file.c    # open and search for "foo"
emacs -v file.c       # open read-only
emacs --version
emacs --help
```

| Option | Meaning |
|---|---|
| `+N`, `-gN` | Go to line N |
| `-sTEXT` | Search for TEXT at startup |
| `-v` | View (read-only) mode |
| `-e` | Edit mode (default) |
| `-n` | Keep NUL bytes in files |
| `-r` | Restricted mode (no shell commands, no file switching) |
| `@FILE` | Run FILE as the startup script instead of `~/.emacsrc` |

---

## How to read the key names

The editor uses standard Emacs notation:

| Notation | Means |
|---|---|
| `C-x` | Hold **Control** and press `x` |
| `M-x` | Hold **Alt** (Meta) and press `x`, **or** press and release **Esc**, then press `x` |
| `C-M-x` | Hold Control and Alt and press `x` (or Esc, then `C-x`) |
| `S-<up>` | Shift + Up arrow |
| `C-x C-f` | Press `C-x`, release, then press `C-f` |
| `C-x b` | Press `C-x`, release, then press plain `b` |
| `RET` `TAB` `DEL` `SPC` `ESC` | Enter, Tab, Backspace, Space, Escape |
| `<up>` `<home>` `<prior>` `<next>` `<f1>` | Arrow, Home, Page Up, Page Down, F1 keys |

Letters after `M-`, `C-x` and `C-c` are not case-sensitive (`C-x B` = `C-x b`).

**Numeric arguments:** `C-u` multiplies the next command by 4 (`C-u C-u` = 16),
`C-u 25 C-n` moves down 25 lines, and `M-5 M-0 C-n` does the same with Alt.
`M--` gives a negative argument.

---

## Every key binding

Inside the editor, `C-h b` shows the live list of all bindings, and `C-h k`
followed by any key tells you what that key does.

### Help

| Key | Command | Description |
|---|---|---|
| `C-h ?` / `C-h h` / `C-h C-h` / `<f1>` | `help` | Quick-reference help buffer (press `q` to leave it) |
| `C-h b` | `describe-bindings` | List every command and its keys |
| `C-h k` / `C-h c` | `describe-key` | Show which command a key runs |
| `C-h a` / `C-h f` / `C-h x` | `apropos-command` | List commands whose name contains a word |
| `C-h m` | `describe-mode` | Show the active modes, indentation and fill column |
| `C-c h` | `help` | Quick-reference help |

### Files

| Key | Command | Description |
|---|---|---|
| `C-x C-f` | `find-file` | Open a file (prompt starts in the current file's folder; TAB completes) |
| `C-x C-r` | `find-file-read-only` | Open a file read-only |
| `C-x C-v` | `find-alternate-file` | Replace this buffer with another file |
| `C-x C-s` / `<f2>` | `save-buffer` | Save the current file |
| `C-x s` / `C-c s` | `save-some-buffers` | Ask to save each modified file (`C-u C-x s` saves all without asking) |
| `C-x C-w` | `write-file` | Save as a new name (ending the name with `/` keeps the file name) |
| `C-x i` | `insert-file` | Insert a file's contents at the cursor |
| `C-c r` | `revert-buffer` | Reload the file from disk |
| `C-x C-q` | `read-only-mode` | Toggle read-only |
| `C-x C-c` | `save-buffers-kill-terminal` | Offer to save modified files, then quit |
| `C-z` / `C-x C-z` | `suspend-emacs` | Suspend to the shell (`fg` to come back) |
| `C-c f` | `find-file` | Alternative open-file key |

### Buffers

| Key | Command | Description |
|---|---|---|
| `C-x b` | `switch-to-buffer` | Switch buffer; TAB completes; RET alone goes to the previous buffer; a new name creates a buffer |
| `C-x C-b` / `C-c b` | `list-buffers` | List all buffers |
| `C-x k` | `kill-buffer` | Close a buffer (RET closes the current one, asks if it is modified) |
| `C-x <right>` | `next-buffer` | Next buffer |
| `C-x <left>` | `previous-buffer` | Previous buffer |
| `M-~` | `not-modified` | Mark the buffer as unmodified |

### Windows

| Key | Command | Description |
|---|---|---|
| `C-x 2` | `split-window-below` | Split the window in two |
| `C-x o` | `other-window` | Move to the next window |
| `C-x 1` | `delete-other-windows` | Keep only this window |
| `C-x 0` | `delete-window` | Close this window |
| `C-x ^` | `enlarge-window` | Make this window taller |
| `C-x -` | `shrink-window` | Make this window shorter |
| `C-M-v` / `M-<next>` | `scroll-other-window` | Scroll the other window down |
| `M-<prior>` | `scroll-other-window-down` | Scroll the other window up |

### Cursor movement

| Key | Command | Description |
|---|---|---|
| `C-f` / `<right>` | `forward-char` | One character right |
| `C-b` / `<left>` | `backward-char` | One character left |
| `C-n` / `<down>` | `next-line` | One line down (keeps the column) |
| `C-p` / `<up>` | `previous-line` | One line up |
| `M-f` / `C-<right>` / `M-<right>` | `forward-word` | One word right |
| `M-b` / `C-<left>` / `M-<left>` | `backward-word` | One word left |
| `C-a` | `beginning-of-line` | Start of line |
| `C-e` / `<end>` | `end-of-line` | End of line |
| `<home>` | `beginning-of-line-text` | VS Code Home: first non-blank character, press again for column 0 |
| `M-m` | `back-to-indentation` | First non-blank character of the line |
| `M-}` / `C-<down>` | `forward-paragraph` | Next paragraph |
| `M-{` / `C-<up>` | `backward-paragraph` | Previous paragraph |
| `M-<` / `C-<home>` | `beginning-of-buffer` | Top of the buffer |
| `M->` / `C-<end>` | `end-of-buffer` | Bottom of the buffer |
| `M-g g` / `M-g M-g` / `C-c g` | `goto-line` | Go to a line number |
| `M-g TAB` / `M-g c` | `move-to-column` | Go to a column |
| `C-M-f` | `forward-sexp` | Jump over the next bracketed expression, string or word |
| `C-M-b` | `backward-sexp` | Jump back over the previous one |
| `C-M-a` | `beginning-of-defun` | Start of the current/previous function |
| `C-M-e` | `end-of-defun` | End of the function |
| `C-c m` | `goto-matching-bracket` | Jump to the matching `(` `)` `[` `]` `{` `}` |
| `M-r` | `move-to-window-line-top-bottom` | Cursor to middle of window; repeat for top, bottom |

### Scrolling and screen

| Key | Command | Description |
|---|---|---|
| `C-v` / `<next>` (PgDn) | `scroll-up-command` | Next screen |
| `M-v` / `<prior>` (PgUp) | `scroll-down-command` | Previous screen |
| `C-l` | `recenter-top-bottom` | Centre the cursor line; repeat to put it at the top, then bottom |
| `C-M-l` | `redraw-display` | Redraw the whole screen |

### Inserting and deleting

| Key | Command | Description |
|---|---|---|
| `RET` | `newline` | New line with smart automatic indentation |
| `C-j` | `newline-and-indent` | Same as RET |
| `C-o` | `open-line` | Insert a newline after the cursor without moving |
| `C-c o` | `open-line-below` | VS Code Ctrl+Enter: start a new indented line below |
| `DEL` (Backspace) | `delete-backward-char` | Delete before the cursor (also removes an empty bracket pair and full indentation steps) |
| `C-d` / `<delete>` | `delete-char` | Delete the character under the cursor (or the selection) |
| `M-d` / `C-<delete>` | `kill-word` | Delete to the end of the word |
| `M-DEL` / `C-M-h` | `backward-kill-word` | Delete to the start of the word |
| `C-k` | `kill-line` | Delete to the end of the line (at the end: join with next line) |
| `C-c k` | `kill-whole-line` | Delete the whole line |
| `M-z` | `zap-to-char` | Delete up to and including a character you type |
| `C-q` | `quoted-insert` | Insert the next key literally (e.g. `C-q TAB`) |
| `M-/` | `dabbrev-expand` | Complete the word from other words in the buffer; repeat for the next candidate |
| `<insert>` | `overwrite-mode` | Toggle overwrite mode |

### Undo and redo

Unlimited undo for every change (typing, pasting, replacing, indenting,
moving lines, case changes...). Undoing back to the last save clears the
"modified" `**` mark, just like VS Code.

| Key | Command | Description |
|---|---|---|
| `C-/` / `C-_` / `C-x u` / `C-x C-_` | `undo` | Undo the last change (`C-u 10 C-/` undoes 10) |
| `M-_` / `C-M-_` / `C-M-/` / `C-c z` | `undo-redo` | Redo what was undone |

### Kill (cut), copy and yank (paste)

| Key | Command | Description |
|---|---|---|
| `C-w` | `kill-region` | Cut the selection |
| `M-w` | `kill-ring-save` | Copy the selection |
| `C-y` | `yank` | Paste the last cut/copied text (replaces an active selection) |
| `M-y` | `yank-pop` | Right after `C-y`: replace it with older cuts (the 64 most recent are kept) |

Consecutive kills (`C-k C-k C-k`) are joined into one piece of text. Pasting
from your terminal (Ctrl+Shift+V / Cmd+V) inserts the text exactly, with no
auto-indentation or bracket pairing (bracketed paste).

### Selection (the region)

| Key | Command | Description |
|---|---|---|
| `C-SPC` / `C-@` | `set-mark-command` | Start a selection at the cursor (press twice to cancel; `C-u C-SPC` jumps to the mark) |
| `S-<arrows>`, `S-<home>`, `S-<end>`, `C-S-<left>/<right>`, `S-<prior>/<next>` | (shift-select) | Select text the VS Code way |
| `C-x h` | `mark-whole-buffer` | Select everything |
| `M-h` | `mark-paragraph` | Select the paragraph |
| `C-x C-x` | `exchange-point-and-mark` | Jump to the other end of the selection |
| `C-g` | `keyboard-quit` | Cancel the selection or any command |

The selection is highlighted. Typing, `DEL`, `C-d`, `RET` and `C-y` replace it.
Typing `(`, `[`, `{`, `"`, `'` or `` ` `` wraps the selection in that pair.

### Line operations (VS Code style)

All of these work on the current line or on every line touched by the selection.

| Key | Command | Description |
|---|---|---|
| `C-c d` / `C-c C-d` / `M-S-<down>` | `duplicate-line` | Duplicate the line(s) below (VS Code Shift+Alt+Down) |
| `C-c u` / `M-S-<up>` | `duplicate-line-up` | Duplicate the line(s) above (VS Code Shift+Alt+Up) |
| `M-<up>` / `C-c p` | `move-line-up` | Move the line(s) up (VS Code Alt+Up) |
| `M-<down>` / `C-c n` | `move-line-down` | Move the line(s) down (VS Code Alt+Down) |
| `C-c k` | `kill-whole-line` | Delete the line (VS Code Ctrl+Shift+K) |
| `C-x C-t` | `transpose-lines` | Swap this line with the previous one |
| `M-^` / `C-c j` | `delete-indentation` | Join this line to the previous one |

### Indentation

| Key | Command | Description |
|---|---|---|
| `TAB` | `indent-for-tab-command` | Indent the line to the right level; inside text, insert one indent; with a selection, indent all selected lines |
| `S-TAB` (`<backtab>`) / `C-c <` | `indent-rigidly-left` | Outdent the line or selection by one level |
| `C-x TAB` / `C-c >` | `indent-rigidly` | Shift the line/selection right by one level (`C-u -4 C-x TAB` shifts left 4 columns) |
| `C-M-\` / `C-c \` | `indent-region` | Re-indent every selected line automatically |
| `C-u 4 TAB` / `C-u 0 TAB` | | Use 4-space (or any number) / tab indentation in this buffer |
| `C-c t` | `set-indentation` | Choose tabs or N spaces for this buffer |

### Comments

| Key | Command | Description |
|---|---|---|
| `M-;` / `C-c ;` / `C-c c` | `comment-line` | Toggle a line comment on the line or selected lines (VS Code Ctrl+/) |

The comment syntax follows the file type: `//` for C, C++, Java, JavaScript,
TypeScript, Go, Rust, C#, Swift, Kotlin, PHP, CSS, JSON...; `#` for Python,
shell, Ruby, Perl, YAML, TOML, Makefiles...; `;;` for Lisp; `--` for SQL, Lua,
Haskell; `%` for TeX; `<!-- -->` for HTML, XML, Markdown.

### Case changes and transposing

| Key | Command | Description |
|---|---|---|
| `M-u` | `upcase-word` | UPPERCASE the next word |
| `M-l` | `downcase-word` | lowercase the next word |
| `M-c` | `capitalize-word` | Capitalize the next word |
| `C-x C-u` | `upcase-region` | UPPERCASE the selection |
| `C-x C-l` | `downcase-region` | lowercase the selection |
| `C-t` | `transpose-chars` | Swap the two characters around the cursor |
| `M-t` | `transpose-words` | Swap the two words around the cursor |
| `C-x C-t` | `transpose-lines` | Swap two lines |

### Whitespace

| Key | Command | Description |
|---|---|---|
| `M-\` | `delete-horizontal-space` | Delete spaces and tabs around the cursor |
| `M-SPC` | `just-one-space` | Leave exactly one space |
| `C-x C-o` | `delete-blank-lines` | Delete the blank lines around the cursor |
| `C-c w` | `delete-trailing-whitespace` | Remove trailing spaces from every line |
| `M-q` | `fill-paragraph` | Re-wrap the paragraph at the fill column |
| `C-x f` | `set-fill-column` | Set the fill column (`C-u 72 C-x f`) |

### Completion

| Key | Where | Description |
|---|---|---|
| `TAB` | any file prompt | Complete the file name instantly (cached directory listing). Completes the longest common part; when ambiguous, a candidate list pops up and filters as you type. Directories get a trailing `/` |
| `TAB` | buffer prompts (`C-x b`, `C-x k`) | Complete buffer names |
| `TAB` / `SPC` / `?` | `M-x` | Complete command names; if nothing starts with what you typed, commands *containing* it are shown |
| `TAB` | mode prompts | Complete mode names |
| `M-/` | in the text | Complete the current word from words in the buffer |
| `~/` | file prompts | Expanded to your home directory |

### Searching and replacing

| Key | Command | Description |
|---|---|---|
| `C-s` / `C-M-s` | `isearch-forward` | Incremental search forward |
| `C-r` / `C-M-r` | `isearch-backward` | Incremental search backward |
| `M-%` / `C-M-%` | `query-replace` | Replace with confirmation |
| `M-x replace-string` | | Replace all without asking |
| `M-x search-forward` / `search-backward` | | Plain (non-incremental) search |

Searching is case-insensitive unless the search text contains a capital
letter (smart case).

### Keyboard macros

| Key | Command | Description |
|---|---|---|
| `C-x (` / `<f3>` | `kmacro-start-macro` | Start recording keys |
| `C-x )` | `end-kbd-macro` | Stop recording |
| `C-x e` | `call-last-kbd-macro` | Play the macro (`C-u 10 C-x e` plays it 10 times) |
| `<f4>` | `kmacro-end-or-call-macro` | Stop recording, or play the macro |

### Toggles and settings

| Key | Command | Description |
|---|---|---|
| `C-c l` | `display-line-numbers-mode` | Show/hide line numbers |
| `C-c a` | `electric-pair-mode` | Turn bracket/quote auto-closing on/off for this buffer |
| `C-c i` | `electric-indent-mode` | Turn automatic indentation on/off for this buffer |
| `C-c t` | `set-indentation` | Tabs or N spaces |
| `<insert>` | `overwrite-mode` | Overwrite mode |
| `C-x C-q` | `read-only-mode` | Read-only mode |

### Shell and miscellaneous

| Key | Command | Description |
|---|---|---|
| `M-x` | `execute-extended-command` | Run any command by name |
| `M-!` | `shell-command` | Run a shell command, output shown in a buffer |
| `M-|` | `filter-buffer` | Pipe the whole buffer through a shell command and replace it with the output |
| `M-=` | `count-words-region` | Count words, lines and characters in the selection |
| `C-x =` | `what-cursor-position` | Line, column, character code and position |
| `C-u` | `universal-argument` | Numeric argument / repeat |
| `C-g` | `keyboard-quit` | Cancel anything |

### Keys inside the minibuffer (prompts)

| Key | Action |
|---|---|
| `RET` | Accept |
| `C-g` / `ESC ESC ESC` | Cancel |
| `TAB` | Complete (see [Completion](#completion)) |
| `<up>` / `M-p` | Previous entry from the history of this kind of prompt |
| `<down>` / `M-n` | Next history entry |
| `<left>` `<right>` `C-b` `C-f` | Move the cursor |
| `C-a` / `<home>`, `C-e` / `<end>` | Start / end of the input |
| `M-b` `M-f`, `C-<left>` `C-<right>` | Move by word |
| `DEL`, `C-d` / `<delete>` | Delete backward / forward |
| `M-DEL` / `C-w` | Delete the previous word (in file prompts: the previous path component) |
| `M-d` | Delete the next word |
| `C-k` | Delete to the end |
| `C-u` | Delete to the start |
| `C-y` | Insert the first line of the last cut text |
| `C-q` / `C-v` | Insert the next key literally |

### Keys inside incremental search

| Key | Action |
|---|---|
| any text | Extend the search string |
| `C-s` / `C-r` | Next / previous match (at the start: reuse the last search) |
| `DEL` | Remove the last character |
| `RET` / `ESC` | Stop at the current match |
| `C-g` | Cancel and return to where you started |
| any other command key (arrows, `C-a`, ...) | Stop searching and run that key |

### Keys inside query-replace

| Key | Action |
|---|---|
| `y` / `SPC` | Replace this one |
| `n` / `DEL` | Skip this one |
| `!` | Replace all remaining without asking |
| `u` | Undo the last replacement |
| `q` / `RET` | Stop here |
| `.` | Stop and go back to where you started |
| `C-g` | Abort |
| `?` | Show this help |

---

## Automatic behaviour (VS Code features)

**Automatic indentation, with the edge cases handled**
- New lines keep the indentation of the line above.
- After `{`, `[`, `(` the new line is indented one more level; in Python, YAML
  and Makefiles the same happens after `:`.
- Pressing RET between a pair, e.g. `{|}`, opens it up: the closing bracket
  goes on its own line at the original level and the cursor sits indented in
  between.
- After `return`, `pass`, `break`, `continue` and `raise` in Python the next
  line is dedented.
- A line starting with `}` `]` `)` is lined up with the line holding the
  matching opening bracket, both when you press RET before it and when you
  type the closing bracket on an empty line.
- Trailing spaces before the cursor and leading spaces after it are cleaned
  up when you split a line, and blank lines don't keep stray indentation.
- Inside `/* ... */` comments in C-like files, new lines continue with ` * `.
- `#` preprocessor lines jump to column 0 in C/C++.
- Makefiles always use real tabs.
- Indentation style (tabs or 2/4/8 spaces) is detected from the file when it
  opens and shown in the mode line; new files use 4 spaces.

**Auto-closing pairs**
- Typing `(` `[` `{` `"` `'` `` ` `` inserts the closing character too, but only
  when the next character is a space, the end of the line or punctuation (as
  VS Code does), so typing in front of existing text doesn't add junk.
- Quotes are not auto-closed after a backslash, and `'` is not doubled after a
  letter (`don't`) or in plain text / Markdown / Lisp files.
- Typing a closing character that is already there just moves over it.
- Backspace between an empty pair, e.g. `(|)`, deletes both.
- With a selection, typing an opening bracket or quote wraps the selection.

**Paste protection.** When text arrives faster than a person can type (a
paste in a terminal without bracketed paste), auto-indentation and pairing
switch off for it, so pasted code keeps its exact layout.

**Other**
- Matching bracket highlighted (bold + underline) when the cursor is on or
  just after a bracket.
- Selection highlighted in reverse video, extended past the line end when it
  covers the newline.
- Line numbers in a left gutter, with the current line's number highlighted.
- Smart Home key (indentation, then column 0).
- Backspace in space-indented code removes a whole indentation step.
- Undo returns to the saved state cleanly (the `**` modified mark disappears).

---

## The mode line

```
-**-  main.c   L42 C8   Top   (Cmode Indent Pair)  Spaces:4  proj/main.c  ---- Sorena Emacs --
```

| Part | Meaning |
|---|---|
| `--` / `**` | Unmodified / modified (`%%` / `%*` = read-only) |
| `main.c` | Buffer name |
| `L42 C8` | Line (from 1) and column (from 0) |
| `Top` / `Bot` / `All` / `NN%` | Position of the window in the buffer |
| `(...)` | Active modes: `Cmode` (C-family file), `Indent` (auto-indent), `Pair` (auto-close), `Over` (overwrite), `View` (read-only), `Wrap`, `Exact` (case-sensitive search), `Magic` (regular expressions) |
| `Spaces:4` / `Tabs` | Indentation used by TAB and auto-indent |

---

## Commands without a key (M-x)

Every command can be run with `M-x name`. Useful ones without a default key:

| Command | Description |
|---|---|
| `replace-string` | Replace all occurrences |
| `search-forward`, `search-backward` | Non-incremental search |
| `open-line-above` | New indented line above (VS Code Ctrl+Shift+Enter) |
| `rename-buffer` | Rename the buffer |
| `set-visited-file-name` | Change the file name the buffer saves to |
| `add-mode`, `delete-mode` | Turn a mode on/off in this buffer (`wrap`, `exact`, `magic`, `over`, `view`, `cmode`, `indent`, `pair`, `asave`) |
| `add-global-mode`, `delete-global-mode` | Same for all new buffers |
| `justify-paragraph` | Fill and justify a paragraph |
| `kill-paragraph` | Delete a paragraph |
| `detab-line`, `entab-line`, `trim-line` | Tabs to spaces, spaces to tabs, trim the line end |
| `insert`, `overwrite-string` | Insert / overwrite with typed text |
| `shell` | Start an interactive shell |
| `shell-command-raw` | Run a shell command on the full terminal |
| `execute-program` | Run a program |
| `bind-to-key`, `unbind-key` | Change key bindings for this session |
| `set` | Set an editor variable (e.g. `$fillcol`) |
| `execute-file`, `execute-buffer`, `execute-command-line` | Run editor scripts |
| `split-window-below`, `resize-window`, `scroll-up-line`, `scroll-down-line` | Window helpers |
| `change-screen-size`, `change-screen-width` | Force a screen size |
| `kill-emacs` | Quit immediately without asking (`C-u M-x kill-emacs`) |
| `quick-exit` | Save every modified file and quit |

`C-h a` searches this list by keyword.

---

## Customising (~/.emacsrc)

At startup the editor runs `~/.emacsrc` if it exists. One command per line:

```
bind-to-key save-buffer "C-c C-s"
bind-to-key duplicate-line "M-d"
bind-to-key kill-whole-line "C-c x"
add-global-mode "exact"
delete-global-mode "pair"
set $fillcol 100
```

Key strings use the same notation as this README (`"C-x C-s"`, `"M-<up>"`,
`"S-<f5>"`, `"C-c d"`, `"<f6>"`). Quote any key string that contains a space.
Mode names: `wrap cmode spell exact view over magic asave indent pair`.

---

## Terminal notes and troubleshooting

- **Alt doesn't work as Meta:** press `Esc` then the key instead, or enable
  "Use Option as Meta key" (macOS Terminal / iTerm2).
- **`C-h` opens help instead of deleting:** that's standard Emacs. If your
  terminal sends `C-h` for Backspace, set it to send `DEL` (`^?`), or add
  `bind-to-key delete-backward-char "C-h"` to `~/.emacsrc` (help stays on `F1`).
- **`C-s` freezes other programs, not this one:** the editor turns off
  terminal flow control, so `C-s` and `C-q` always reach it.
- **`C-/`, `C-M-/`:** most terminals send them as `C-_` and `C-M-_`, which are
  bound to the same commands.
- **Shift/Alt + arrows:** work in xterm, GNOME Terminal, Konsole, kitty,
  Alacritty, iTerm2, Windows Terminal and tmux (with `xterm-keys on`).
- **Started the wrong emacs?** Run `which emacs`; it should print
  `/usr/local/bin/emacs`. Then `hash -r`.
- If the terminal closes or receives a hang-up signal, modified buffers are
  saved automatically before exiting.

---

Sorena Emacs is written and maintained by **Sorena Arshiyan**.
