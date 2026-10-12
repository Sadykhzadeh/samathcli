<br/>
<p align="center">
  <h3 align="center">SaMathCLI</h3>

  <p align="center">
    CLI-wrapper of SaMathGen, a great tool for educational purposes
    <br/>
    <a href="https://github.com/Sadykhzadeh/samathcli/issues">Report Bug</a>
    .
    <a href="https://github.com/Sadykhzadeh/samathcli/issues">Request Feature</a>
  </p>
</p>

![Downloads](https://img.shields.io/github/downloads/Sadykhzadeh/samathcli/total) ![Contributors](https://img.shields.io/github/contributors/Sadykhzadeh/samathcli?color=dark-green) ![Forks](https://img.shields.io/github/forks/Sadykhzadeh/samathcli?style=social) ![Stargazers](https://img.shields.io/github/stars/Sadykhzadeh/samathcli?style=social) ![Issues](https://img.shields.io/github/issues/Sadykhzadeh/samathcli) ![License](https://img.shields.io/github/license/Sadykhzadeh/samathcli) 

## Table Of Contents

* [Usage](#usage)
* [Contributing](#contributing)
* [Authors](#authors)
* [Acknowledgements](#acknowledgements)

## Usage

* Download repo
* run `make`
* That's it! Type `./samath 5` to get five questions

`make clean` removes the binary and the object files.

### Options

`./samath [options] QUESTIONS`, with the options before or after the number:

| Option | Meaning |
| --- | --- |
| `-t`, `--time SECONDS` | Give each question that long to answer, 1 to 3600. The seconds left count down on the prompt; a question that runs out counts as a mistake and is listed as `(time ran out)`. |
| `-s`, `--seed NUMBER` | Ask the same questions every time, 0 to 4294967295: a class that runs `./samath -s 2024 10` gets the same ten questions. The seed goes to the API, so it has to be a [SaMathAPI](https://got.az/azer/samathapi) that takes `seed`; an older one ignores it. |
| `-p`, `--practice` | When the run is over and there are mistakes, ask them again, round after round, until each has been put right; the result then says how many rounds it took. `Q` stops the practice, and a run that was stopped does not go on to it. The score above it is still the first attempt. |
| `-l`, `--length N` | Ask for questions of `N` numbers each, 2 to 12. Without it the API picks a length from its own range. |
| `-b`, `--brackets` | Ask for questions with brackets. Without it the API's own setting decides. |
| `-h`, `--help` | Print the options. |

`./samath -t 10 20` is twenty questions with ten seconds each, and
`./samath -l 6 -b 10` is ten harder ones. Question `n` of a run is the one the
API gives for `seed + n - 1`, so the questions are the same however they were
fetched. The seed, the length and brackets are passed on to the API, which
has to be a SaMathAPI that takes them; an older one ignores them.

### Answering

At a terminal one key is one answer: `A`, `B`, `C` or `D`, in either case,
with no Enter. `Q` or Ctrl-D stops the run and shows the result so far; any
other key, arrow keys included, is ignored. When the input is a pipe or a
file the same letters are read from it as they come, which makes a run
scriptable: `printf 'abcd' | ./samath 4`.

### The result

When the run ends you get the mistakes, each with the right answer and the
one that was given, and one line with the score and the time:

```
Task 4. 2+3*4	= 14	(you said 20)
Task 9. 18/3-1	= 5	(time ran out)

📊 7 of 10 right (70%), 1 min 12 s, 7.2 s a question
```

The time is the time spent over the questions, from showing one to the key
that answered it; waiting for the server is not counted.

### Requirements

* a C11 compiler (`gcc` or `clang`) and `make`
* `curl` on `PATH`
* a running [SaMathAPI](https://github.com/Sadykhzadeh/samathapi) instance

### Configuration

| Variable | Default | Meaning |
| --- | --- | --- |
| `SAMATH_API_URL` | `http://localhost:3000/gen?c=true` | Where questions are fetched from. It is handed to `curl` as it is. |

### How questions are fetched

The whole run is asked for in one request, `...&count=20` for twenty
questions and in batches of a hundred past that, so a quiz costs one round
trip rather than one per question. Against a server on the same machine, twenty questions
went from 193 ms to 6 ms. A SaMathAPI that does not know `count` sends one
question back, and the run carries on with a request per question, as it
always did. Nothing is written to disk, and questions that are not
in the API's format are ignored. A server that stops answering partway
through ends the run there: the result covers the questions answered so
far, and `samath` exits with status 1.

## Development

`make clean check` rebuilds everything with warnings as errors and runs the
tests in `test/`: unit checks of the parser, then the program itself against a
stand-in for `curl`, then (when `python3` is there) on a pseudo-terminal, which
is the only way to see single-key input working. No server is needed. To
check for memory errors as well:

```sh
make clean check CFLAGS='-std=c11 -g -O1 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined'
```

## Contributing

Contributions are what make the open source community such an amazing place to be learn, inspire, and create. Any contributions you make are **greatly appreciated**.
* If you have suggestions for adding or removing projects, feel free to [open an issue](https://github.com/Sadykhzadeh/samathcli/issues/new) to discuss it, or directly create a pull request after you edit the *README.md* file with necessary changes.
* Please make sure you check your spelling and grammar.
* Create individual PR for each suggestion.
* Please also read through the [Code Of Conduct](https://github.com/Sadykhzadeh/samathcli/blob/main/CODE_OF_CONDUCT.md) before posting your first idea as well.

### Creating A Pull Request

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## Authors

[Azer Sadykhzadeh](https://github.com/Sadykhzadeh)

## Acknowledgements

* [SaMathAPI](https://github.com/Sadykhzadeh/samathapi)
* [SaMathGen](https://github.com/Sadykhzadeh/samathgen)
