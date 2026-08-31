# Notes

The drain fix is deliberately local to the cdev mirror: ring emptiness alone
does not imply that an already-consumed asynchronous backend operation has
retired. A backend without cancellation leaves that operation to its event
contract and rejects Reset/Remove quiesce until a later lifecycle policy defines
the production wait or replacement transaction.
