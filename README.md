# modbus-simulator
Modbus master and slave simulator


## modbus simulator usage

Create a TCP simulator

    $ tcp <port> <slave> <no.CO> <no.DI> <no.HR> <no.IR>

Create a RTU simulator
  
    $ rtu <serial> <mode> <baud> <slave> <no.CO> <no.DI> <no.HR> <no.IR>

Print list of simulated TCP IP and RTU serial

  $ print

Print all slaves of a TCP or RTU port

    $ print <idx>

Print values of all registers and coils of a slave

    $ print <idx> <slave>

Print values of registers and coils of specific range of a slave

    $ print <idx> <slave> <start> <end>

Set values of a coil or register

    $ set <idx> <slave> <type> <addr> <value>
    
| Type | Description |
| ---- | --- |
| 0 | Coil |
| 1 | Discrete Input |
| 2 | Holding Register |
| 3 | Input Register |
