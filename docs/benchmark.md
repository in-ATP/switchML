# Benchmark


## Requirment - Run with Signal Switch

In this experiment, 2 physical workers and 1 switch is used.

## Getting Started
```
$ git clone https://github.com/ATP-NSDI/switchML.git
```

### Run Tofino Switch 

#### Compile P4 Program and Start the Tofino Model (Terminal1)
If you are using physical switch, compile the switch program then jump to Terminal 2 directly.
```
$ $TOOLS/p4_build.sh $REPO_SWITCHML/p4src/switchml.p4
```
```
# (Optional) for software Tofino behavior model
$ cd $SDE/run_tofino_model.sh -p switchml
```
#### Load Specified Switch Program (Terminal2)
```
$ cd $SDE
```
```
$ $SDE/run_switchd.sh -p switchml
```
#### Enable Ports and Install Entries (Terminal3)
```
$ $SDE/run_p4_tests.sh -t $REPO_SWITCHML/ptf/ -p switchml 
```
```
$ $TOOLS/run_pd_rpc.py -p switchml $REPO_SWITCHML/run_pd_rpc/setup.py 
```

### Compile and Run Workers
```
$ cd $REPO_SWITCHML/client/
```
```
$ make
```
#### Run Worker1 (Terminal4)
```
# Usage: ./app [MyID] [Num of Worker] [AppID]
$ sudo ./app 0 2 1
```
#### Run Worker2 (Terminal5)
```
# Usage: ./app [MyID] [Num of Worker] [AppID]
$ sudo ./app 1 2 1
```

Then you can switch to Terminal 4/5 to see the bandwidth report.
