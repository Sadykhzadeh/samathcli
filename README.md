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

### Answering

At a terminal one key is one answer: `A`, `B`, `C` or `D`, in either case,
with no Enter. `Q` or Ctrl-D stops the run and shows the result so far; any
other key, arrow keys included, is ignored. When the input is a pipe or a
file the same letters are read from it as they come, which makes a run
scriptable: `printf 'abcd' | ./samath 4`.

### The result

When the run ends you get the mistakes, each with the right answer, and one
line with the score and the time:

```
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
in the API's format are ignored.

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
