# input-based demo file regression tests
* these are demo files which consist of player inputs rather than game element positions (which is what the classic .dem files encode)
* these are meant to form a set of regression tests so we can replay them and check the final state produced. with a large enough body of demo files we should be able to lock down game engine behavior and enable larger refactoring passes, such as to de-duplicate d1/ and d2/

# todo
* remove debug state tracing from the early demos
