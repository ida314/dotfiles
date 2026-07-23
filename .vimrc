call plug#begin('~/.vim/plugged')

" Markdown live preview in the browser
Plug 'iamcco/markdown-preview.nvim', { 'do': { -> mkdp#util#install() }, 'for': ['markdown', 'vim-plug'] }

" Coding basics
Plug 'tpope/vim-surround'
Plug 'tpope/vim-commentary'
Plug 'tpope/vim-fugitive'
Plug 'airblade/vim-gitgutter'
Plug 'dense-analysis/ale'
Plug 'junegunn/fzf'
Plug 'junegunn/fzf.vim'
Plug 'editorconfig/editorconfig-vim'

" Completion / snippets
Plug 'neoclide/coc.nvim', {'branch': 'release'}
Plug 'SirVer/ultisnips'
Plug 'jiangmiao/auto-pairs'

" Markdown
Plug 'bullets-vim/bullets.vim'
Plug 'SidOfc/mkdx'
Plug 'iamcco/markdown-preview.nvim', { 'do': 'cd app && yarn install' }

call plug#end()

set mouse=a

" markdown-preview: :MarkdownPreview / :MarkdownPreviewStop / :MarkdownPreviewToggle
" let g:mkdp_auto_start = 0
