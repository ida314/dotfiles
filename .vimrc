call plug#begin('~/.vim/plugged')

" Markdown live preview in the browser
Plug 'iamcco/markdown-preview.nvim', { 'do': { -> mkdp#util#install() }, 'for': ['markdown', 'vim-plug'] }

call plug#end()

" markdown-preview: :MarkdownPreview / :MarkdownPreviewStop / :MarkdownPreviewToggle
" let g:mkdp_auto_start = 0
