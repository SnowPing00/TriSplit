AI-Assisted Development: Parts of the code in this project were written and modified with the assistance of Artificial Intelligence (AI).

TriSplit: A 3-Stream Separation-based Compression Engine
TriSplit is not a general-purpose compressor but a highly specialized compression engine for specific data patterns. The core philosophy of this project is "Divide, Transform, and Conquer."

Instead of directly compressing data, it intelligently separates the original data into three sub-streams with entirely different statistical properties. Each separated stream is then transformed into an optimal format for a subsequent entropy coder (rANS) to achieve maximum efficiency. This approach aims to expose the hidden structures within the data and apply the most suitable compression method to each structure, thereby achieving a high compression ratio.

Detailed Explanation of the 3-Stream Separation Method
TriSplit treats the original byte stream as a sequence of 2-bit symbols (00, 01, 10, 11) and rearranges them into three streams based on their statistical characteristics.

Stream 1: value_bitmap
Purpose: To store the value information of 01 and 10 symbols separately.

Generation Method:

Whenever a 01 or 10 symbol is encountered in the original data, the second bit of that symbol is recorded sequentially in this stream.

A 0 is stored for 10, and a 1 is stored for 01.

Characteristics:

This stream becomes a very simple binary stream where the frequencies of 0s and 1s are nearly identical.

Entropy coders like rANS can compress such uniformly distributed data very efficiently.

Stream 2: reconstructed_stream
Purpose: To store the information of 00/11 symbols and the positional information of 01/10 symbols.

Generation Method:

When a 00 or 11 symbol is encountered in the original data, its first bit (0) is stored as is.

When a 01 or 10 symbol is encountered, a "marker" symbol (1) is stored to indicate its position.

Characteristics:

This stream consists of only two types of bits: 0 (from original 00/11 symbols) and 1 (markers).

It contains the structural information of the data and is used in conjunction with the value_bitmap to restore the 01/10 symbols.

Stream 3: auxiliary_mask
Purpose: To record the positions of the rarer symbol between 00 and 11.

Generation Method:

Before compression begins, the frequencies of 00 and 11 are calculated to determine which symbol is rarer.

While generating the reconstructed_stream, whenever a 00 or 11 symbol is encountered, if that symbol is the "rare symbol," a 1 is recorded in this mask stream. A 0 is recorded in all other positions.

Characteristics:

This stream becomes a sparse bitmask, consisting mostly of 0s with occasional 1s.

Such sparse data is the ideal format for an rANS coder to achieve extremely high compression ratios.

Compression and Decompression Process
Compression:

The SeparationEngine separates the original data into the three streams described above.

The rANS_Coder individually compresses each stream according to its statistical properties.

The three compressed streams and the necessary metadata for decompression (original size, compressed size of each stream, etc.) are bundled into a single block and stored.

Decompression:

The metadata is read from the compressed block to identify the boundaries of each stream.

The rANS_Coder decompresses each stream in parallel to restore the three original streams.

The SeparationEngine combines the three restored streams to perfectly reconstruct the final original data.

Why is this Method Effective?
Applying a single compression algorithm to all data can be inefficient. TriSplit "deconstructs" the data from multiple perspectives to reveal the intrinsic properties of each part.

value_bitmap separates pure information.

reconstructed_stream separates structural information.

auxiliary_mask separates exceptional information.

These separated and transformed streams each take on a form that is highly advantageous for entropy coding, thus achieving high overall compression efficiency.