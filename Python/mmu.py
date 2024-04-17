class MemoryRangeError(ValueError):
    pass


class ReadOnlyError(TypeError):
    pass


class MMU:
    def __init__(self, blocks):
        
        # Memory allocated to the 6502.
        self.memory = bytearray(65536)
        
        # Map that specifies how the memory is to be used. A zero
        # specifies that the byte can be read or written to. Any 
        # other number will reference a callback routine that the 
        # memory access must pass through.
        self.memmap = bytearray(65536)
        
        # Keep track of any callback methods.
        self.callbacks = {}
        self.callbacks[1] = self.readonly
        
        """
        Initialize the MMU with the blocks specified in blocks.  blocks
        is a list of 6-tuples, (start, length, readonly, value, valueOffset,
        callback).

        See `addBlock` for details about the parameters.
        """

        # The blocks passed are processed and the memory and memmap 
        # arrays will be updated.
        for b in blocks:
            self.addBlock(*b)

    def reset(self):
        """
        In all writeable memory reset the values to zero.
        """
        for i in range(len(self.memory)):
            if self.memmap[i] == 0:
                self.memory[i] = 0
    
    # if a memory address is marked read only call this method to access 
    # the memory.      
    def readonly(self, addr, value=None):
        if value != None:
            # Trying to write. Just post a message.
            print("Trying to write to a read only address:", hex(addr), hex(value))
        else:
            # Trying to read. Just go ahead.
            return self.memory[addr]        

    def addBlock(self, start, length, readonly=False, value=None, valueOffset=0, callback=None):
        """
        Process the memory and memory map with the given start address and length; 
        whether it is readonly or not; and the starting value as either
        a file pointer, binary value or list of unsigned integers.  If the
        block overlaps with an existing block an exception will be thrown.

        Parameters
        ----------
        start : int
            The starting address of the block of memory
        length : int
            The length of the block in bytes
        readOnly: bool
            Whether the block should be read only (such as ROM) (default False)
        value : file pointer, binary or lint of unsigned integers
            The intial value for the block of memory. Used for loading program
            data. (Default None)
        valueOffset : integer
            Used when copying the above `value` into the block to offset the
            location it is copied into. For example, to copy byte 0 in `value`
            into location 1000 in the block, set valueOffest=1000. (Default 0)
        keyboard : function
            If a block is the keyboard, use the keyboard read and write instead
            of memory access.
        """

        # See if the block of memory is read only.
        if readonly:
            for i in range(length):
                self.memmap[start+i] = 1
                
        if callback != None:
            # See if the callback has already been defined.
            key = None
            for k in self.callbacks.keys():
                if self.callbacks[k] == callback:
                    key = k
                    break
            # Not defined so create a new entry.
            if key == None:
                key = len(self.callbacks) + 1
                self.callbacks[key] = callback
            
            # Mark the range of memory with the call back key.
            for i in range(length):
                self.memmap[start+i] = key
            

        # Process memory values.
        if type(value) == list:
            for i in range(len(value)):
                self.memory[start+i+valueOffset] = value[i]

        elif value is not None:
            data = value.read().split()
            for i in range(len(data)):
                self.memory[start+i+valueOffset] = int(data[i], 16)


    def write(self, addr, value):
        """
        Write a value to the given address if it is writeable.
        """
        if self.memmap[addr] != 0:
            callback = self.callbacks[self.memmap[addr]]
            callback(addr, value & 0xff)
        else:
            self.memory[addr] = value & 0xff

    def read(self, addr):
        """
        Return the value at the address.
        """
        if self.memmap[addr] != 0:
            return self.callbacks[self.memmap[addr]](addr, None)
        else:
            return self.memory[addr]
        

    def readWord(self, addr):
        return (self.read(addr+1) << 8) + self.read(addr)
