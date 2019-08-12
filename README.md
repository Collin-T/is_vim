# Is vim
This is a simple project I created to go along with vim-tmux-navigator

## Purpose
vim-tmux-navigator checks if the current tmux pane is using a simple shell script, which usually works fine.
However, this script can't handle nested shells (such as pipenv shell), and so using tmux-vim-navigator will break certain workflows.
This is a **known issue**, and although there are [some solutions](https://github.com/christoomey/vim-tmux-navigator/issues/195#issuecomment-384983711), I wasn't satisfied with them.
In particular, I found that the [more complex shell script solution](https://gist.github.com/akselsjogren/35aec0af39e53319e12a3e1432da4d4e) took about 0.24s to run on my machine, which was very noticable in my workflow.
Creating a simple python script made the issue better, but the solution still took to 0.09s on my machine, which is better but still noticable.
Since 0.03s come from running ps, and 0.05s come from python's startup and parsing time, it doesn't seem like major performance improvements can be gained in python.

This led me to create this C program to run the process.
It reads information from ps and follows the children of processes in a given tty to try to find vim/nvim.
This program runs in 0.03 seconds on my machine, almost identical to the time to execute ps, and is fast enough that the latency in the operation is almost unnoticable to me.

Both the original python script and the C program are included, although I highly recommend using the C program.
To use the python script, place the script in a directory in your path, and change the command is\_vim to ```is_vim="python is_vim.py #{pane_tty}"```.
To use the C program, compile the program and place the program in your path. Then change the commnad is\_vim to ```is_vim="is_vim #{pane_tty}"```.
