# VoTE explanation experiments
This repository holds the source code and data needed to reproduce results from the paper
[Finding Minimum-Cost Explanations for Predictions made by Tree Ensembles][paper].
The [logs and raw results][results] used in the paper are also provided.

## Prerequisites
On Debian-flavored operating systems (tested with Ubuntu 24.04.2), you can invoke
the following commands to install dependencies, generate makefiles, and compile
 the source code.
```console
john@localhost:VoTE-explain-experiments$ sudo apt-get install autoconf libtool build-essential
john@localhost:VoTE-explain-experiments$ sudo apt-get install python3-cffi python3-setuptools python3-dev python3-pip
john@localhost:VoTE-explain-experiments$ sudo apt-get install parallel python3-numpy python3-matplotlib python3-seaborn
john@localhost:VoTE-explain-experiments$ python3 -m pip install --user python-sat
john@localhost:VoTE-explain-experiments$ ./bootstrap.sh
john@localhost:VoTE-explain-experiments$ ./configure
john@localhost:VoTE-explain-experiments$ make
```

## Running experiments
The results used in the paper can be reproduced by running the following set of commands.
```console
john@localhost:VoTE-explain-experiments$ cd support/xai-paper
john@localhost:VoTE-explain-experiments/support/xai-paper/$ ./01-run-experiments.sh
john@localhost:VoTE-explain-experiments/support/xai-paper/$ ./02-gen-paper-figures.sh
```

[paper]: https://arxiv.org/abs/2303.09271
[results]: https://github.com/john-tornblom/VoTE-explain-experiments/tree/master/support/xai-paper/results
