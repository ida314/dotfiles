# If you come from bash you might have to change your $PATH.
# export PATH=$HOME/bin:$HOME/.local/bin:/usr/local/bin:$PATH
#

export PATH=/opt/homebrew/bin:/usr/local/bin:/System/Cryptexes/App/usr/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Library/Apple/usr/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/local/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/appleinternal/bin
export PATH=/opt/homebrew/bin:/usr/local/bin:/System/Cryptexes/App/usr/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Library/Apple/usr/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/local/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/appleinternal/bin
export PATH=/opt/homebrew/bin:/usr/local/bin:/System/Cryptexes/App/usr/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Library/Apple/usr/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/local/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/bin:/var/run/com.apple.security.cryptexd/codex.system/bootstrap/usr/appleinternal/bin
export PATH="$HOME/.cargo/bin:$PATH"

export PATH="$HOME/bin:$PATH"

source ~/.zsh_profile

#vim bindings commandline
bindkey -v

#color for ls and other commands
export CLICOLOR=1
export LSCOLORS=GxFxCxDxBxegedabagaced
alias ls='ls --color=auto'  # For Linux
# or
#alias ls='ls -G'            # For macOS
#
#


alias config='/usr/bin/git --git-dir=$HOME/.cfg/ --work-tree=$HOME'




# l = ls -la 
alias l='ls -G -la'

alias vim='nvim'

# git branch tracking
autoload -Uz vcs_info
precmd() { vcs_info }

setopt prompt_subst
PROMPT='%F{cyan}%n@%m%f %F{green}${vcs_info_msg_0_}%f %# '
zstyle ':vcs_info:git:*' formats '(%b)'

