MEMORY
{
  CODE (rx) : ORIGIN = 0x00000000, LENGTH = 8K
  DATA (rw) : ORIGIN = 0x00000000, LENGTH = 8K
  RESOURCE (rx) : ORIGIN = 0x12c00, LENGTH = 1K
}


/* Section Definitions */

SECTIONS
{

  /* first section is .text which is used for code */

.text :
  {
	start = .;
                *(.text .text.*)
  } >CODE =0

.resource_table :
{
	*(.resource_table)
} >RESOURCE = 0
}

ENTRY (start)

