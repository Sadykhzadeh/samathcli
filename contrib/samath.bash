# Bash completion for samath. Source it, or copy it to
# ~/.local/share/bash-completion/completions/samath.

_samath() {
  local cur=${COMP_WORDS[COMP_CWORD]} prev=${COMP_WORDS[COMP_CWORD-1]}
  local options='--time --seed --practice --length --brackets --no-brackets
    --max --operators --range --no-negatives --negatives --feedback --help
    --version'
  COMPREPLY=()
  case $prev in
    -o|--operators)
      # One name at a time; after a comma, the next.
      local head= tail=$cur
      case $cur in *,*) head=${cur%,*},; tail=${cur##*,} ;; esac
      COMPREPLY=($(compgen -P "$head" -W 'add subtract multiply divide' -- "$tail"))
      return ;;
    -t|--time|-s|--seed|-l|--length|-m|--max|-r|--range)
      return ;;
  esac
  case $cur in
    -*) COMPREPLY=($(compgen -W "$options" -- "$cur")) ;;
  esac
}

complete -F _samath samath ./samath
