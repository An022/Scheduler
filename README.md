# Scheduler
A programming lab with professor Hubertus Franke. 

In this lab, we investigate how various scheduling policies are implemented and what impact they have on a group of processes or threads running on a system. Discrete event simulation is going to be used to implement the system (DES). The operation of a system is portrayed as a chronological sequence of events in discrete-event simulation.   

Each event takes place at a specific moment in time and represents a shift in the system's state. This suggests that rather than incrementing time continuously, the system advances in time by specifying and carrying out the events (state transitions) and by discretizing temporal progression between the events. Events are handled after being removed from the event queue in chronological order, and they may produce new events in the present or in the future. Keep in mind that DES has nothing to do with OS; it is just a fantastic, general method for simulating system behavior and time travel that may be used in a variety of system simulation scenarios.
