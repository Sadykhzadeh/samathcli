# Bash completion for samath. Source it, or copy it to
# ~/.local/share/bash-completion/completions/samath.

_samath() {
  local cur=${COMP_WORDS[COMP_CWORD]} prev=${COMP_WORDS[COMP_CWORD-1]}
  local options='-t -s -p -l -b -B -m -o -r -n -N -f -P -h --time --seed
    --practice --length --brackets --no-brackets --max --operators --range
    --no-negatives --negatives --feedback --print --help --version'
  COMPREPLY=()
  # Bash splits --operators=add at the equals sign, so the option is two words back.
  if [ "$prev" = = ]; then prev=${COMP_WORDS[COMP_CWORD-2]}; fi
  case $prev in
    -o|--operators)
      # One name at a time; after a comma, the next one not named yet.
      local head= tail=$cur named= name words=
      case $cur in *,*) head=${cur%,*},; tail=${cur##*,}; named=${cur%,*} ;; esac
      for name in add subtract multiply divide; do
        case ",$named," in *",$name,"*) ;; *) words="$words $name" ;; esac
      done
      COMPREPLY=($(compgen -P "$head" -W "$words" -- "$tail"))
      return ;;
    -t|--time|-s|--seed|-l|--length|-m|--max|-r|--range)
      return ;;
  esac
  case $cur in
    -*) COMPREPLY=($(compgen -W "$options" -- "$cur")) ;;
  esac
}

complete -F _samath samath ./samath
