#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "qemu/event_notifier.h"
#include "sysemu/kvm.h"


#define TYPE_PCI_JERIES_DEV "pci-jeriesdev"
#define PCI_JERIES_DEV(obj)     OBJECT_CHECK(PCIJeriesDevState, (obj), TYPE_PCI_JERIES_DEV)
#define JERIES_MMIO_SIZE 16
#define JERIES_IO_SIZE	32

/*
PCIJeriesDevState: This is a structure that defines the state of the device. It mostly contains indicators that represent various functions and/or operations. 
*/
typedef struct PCIJeriesDevState {
	PCIDevice parent_obj;
	MemoryRegion io;
	MemoryRegion mmio;
	qemu_irq irq;
	unsigned int dma_size;
	char *dma_buff;
	int irq_thrown;
	int id;
	int awake;
} PCIJeriesDevState;




/*
MEMORY MAPPED I/O WRITE function

This function is responsible of how the MMIO write operation is handled inside the device. 
opaque:	Void pointer that is cast to DeviceState pointer. 
addr: 	write address offset. Starts at 0.
value:	Desired data to be written using MMIO.
size:	Size of written data. (Depends on write function used, iowrite8, iowrite16, iowrite32).

Implementation:
Address is checked using a switch statement. 
This device has a register which represents the opertaing state of the device at add:offset = 0, 
depending on the read value (0 or 1) the device's state can be altered between awake or asleep.

The device's ID Register is set to desired value when specifying desired address to be at addr:offset = 4.

In case a different address was requested, the default statement is invoked to infrom that MMIO operation can't be used in that memory region.
*/
static void jeries_mmiowrite(void *opaque, hwaddr addr, uint64_t value, unsigned size){
	PCIJeriesDevState *state = (PCIJeriesDevState *) opaque;
	PCIDevice *pci_dev = (PCIDevice *) opaque;
	printf("MMIO write Ordered, addr=%x, value=%lu, size=%d\n",(unsigned) addr, value, size);
	switch(addr){
		case 0:
			if(value == 0){
				state->awake = 0;
				break;
			}
			else{
				state->awake = 1;
				break;
			}
		case 4:
			state->id = value;
			break;
		default:
			printf("MMIO not writable or not used\n");
	}
	state->irq_thrown = 1;
	pci_irq_assert(pci_dev);
}
/*
MEMORY MAPPED I/O READ function

This function is responsible of how the MMIO read operation is handled inside the device. 
opaque:	Void pointer that is cast to DeviceState pointer. 
addr: 	write address offset. Starts at 0.
size:	Size of read data. (Depends on read function used, ioread8, ioread16, ioread32).

Implementation:
Address is checked using a switch statement. 
This device has a register which represents the opertaing state of the device at add:offset = 0, 
this value is returned when the register's value is requested. 

The device's ID register value is returned  when specifying desired address to be at addr:offset = 4.

In case a different address was requested, the default statement is invoked to infrom that MMIO operation can't be used in that memory region
*/
static uint64_t jeries_mmioread(void *opaque, hwaddr addr, unsigned size){
	PCIJeriesDevState *state = (PCIJeriesDevState *) opaque;
	printf("MMIO read Oredered, addr=%x, size=%d\n", (unsigned) addr, size);
	
	switch(addr){
		case 0:
			return state->awake;
		case 4:
			printf("ID\n");
			return state->id;
			break;
		default:
			printf("MMIO not used\n");
			return 0;
	}
}
/*
jeries_mmio_ops: this is a structure which contains invocations to the desired read and write functions to handle the MMIO operations. How data is described in memory in terms of edianness, and the maximum and minimum access sizes required for alignment purposes.  
*/
static const MemoryRegionOps jeries_mmio_ops = {
	.read = jeries_mmioread,
	.write = jeries_mmiowrite,
	.endianness = DEVICE_NATIVE_ENDIAN,
	.valid = {
		.min_access_size = 4,
		.max_access_size = 4,
	},
};
/*
Port I/O WRITE function

This function is responsible of how the PIO write operation is handled inside the device. 
opaque:	Void pointer that is cast to DeviceState pointer and to PCIDevice pointer. 
addr: 	write address offset. Starts at 0.
value:	Desired data to be written using PIO.
size:	Size of written data. (Depends on write function used, outb, outw, outl).

Implementation:
Address is checked using a switch statement. 
This device has a port at add:offset = 0, 
depending on the read value (0 or 1) the device can assert an interrupt and deassert an interrupt.

The device can simulate a DMA operation when specifying desired address to be at addr:offset = 4.

In case a different address was requested, the default statement is invoked to infrom that PIO operation can't be used.
*/
static void jeries_iowrite(void *opaque, hwaddr addr, uint64_t value,unsigned size){
	int i;
	PCIJeriesDevState *state = (PCIJeriesDevState *) opaque;
	PCIDevice *pci_dev = (PCIDevice *) opaque;
	
	printf("Write Ordered, addr=%x, value=%lu, size=%d\n", (unsigned) addr, value, size);
	
	switch(addr){
		case 0:
			if(value){
				printf("irq assert\n");
				state->irq_thrown = 1;
				pci_irq_assert(pci_dev);
			}
			else{
				printf("irq deassert\n");
				state->irq_thrown = 0;
				pci_irq_deassert(pci_dev);
			}
			break;
		case 4:
			for(i = 0; i < state->dma_size; i++){
				state->dma_buff[i] = rand();
			}
			cpu_physical_memory_write(value, (void *) state->dma_buff, state->dma_size);
			break;
		default:
			printf("IO not used\n");
	}

	state->irq_thrown = 1;
	pci_irq_assert(pci_dev);
}
/*
Port I/O READ function

This function is responsible of how the PIO read operation is handled inside the device. 
opaque:	Void pointer that is cast to DeviceState pointer. 
addr: 	read address offset. Starts at 0.
size:	Size of read data. (Depends on read function used, inb, inw, inl).

Implementation:
Address is checked using a switch statement. 
This device has a port at add:offset = 0, 
a value that tells if an interrupt has been thrown is returned.

In case a different address was requested, the default statement is invoked to infrom that PIO operation can't be used.
*/
static uint64_t jeries_ioread(void *opaque, hwaddr addr, unsigned size){
	PCIJeriesDevState *state = (PCIJeriesDevState *) opaque;
	printf("Read Orderedm addr=%x, size=%d\n", (unsigned) addr, size);

	switch(addr){
		case 0:
			return state->irq_thrown;
			break;
		default:
			printf("IO not used\n");
			return 0;
	}
	
}
/*
jeries_io_ops: this is a structure which contains invocations to the desired read and write functions to handle the PIO operations. How data is described in memory in terms of edianness, and the maximum and minimum access sizes required for alignment purposes.  
*/
static const MemoryRegionOps jeries_io_ops = {
	.read = jeries_ioread,
	.write = jeries_iowrite,
	.endianness = DEVICE_NATIVE_ENDIAN,
	.valid = {
		.min_access_size = 4,
		.max_access_size = 4,
	},
};
/*
Function to set the device's initial state, register the IO regions in memory, and set interrupt pin for the device.
*/
static int pci_jeriesdev_init(PCIDevice *pci_dev){
	PCIJeriesDevState *state = PCI_JERIES_DEV(pci_dev);
	state->dma_size = 0x1FFFF * sizeof(char);
	state->dma_buff = malloc(state->dma_size);
	state->id = 0x1337;
	state->irq_thrown = 0;
	state->awake = 1;
	uint8_t *pci_conf;
	memory_region_init_io(&state->mmio, OBJECT(state), &jeries_mmio_ops, state, "jeries_mmio", JERIES_MMIO_SIZE);
	memory_region_init_io(&state->io, OBJECT(state), &jeries_io_ops, state, "jeries_io", JERIES_IO_SIZE);
	pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &state->io);
	pci_register_bar(pci_dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &state->mmio);
	pci_conf = pci_dev->config;
	pci_conf[PCI_INTERRUPT_PIN] = 0x02;
	printf("Jeries's Device is loaded\n");
	return 0;
}
/*
Function to uninitialize the device, and remove the device. 
*/
static void pci_jeriesdev_uninit(PCIDevice *dev){
	PCIJeriesDevState *state = (PCIJeriesDevState *) dev;
	free(state->dma_buff);
	printf("Jeries's Device is Unloaded\n");
}
/*
Function to reset the device.
*/
static void qdev_pci_jeriesdev_reset(DeviceState *dev){
	printf("Resetting Jeries's Module\n");
}
/*
Function to configure the device in order to be recognized properly by the PCI BUS.
realize:	invoke device initialization function.
exit:		function invoked when device is removed.
Device ID:	ID of the device as set by the manufacturer. 
Vendor ID:	ID of the vendor of the device.
Class ID:	ID of the class that the device falls under.
revision:	Revision number of the device.
Desc:		Description of the device.
reset:		Reset function to be invoked when device is reset.
*/
static void pci_jeriesdev_class_init(ObjectClass *class, void *data){
	DeviceClass *dc = DEVICE_CLASS(class);
	PCIDeviceClass *pci_class = PCI_DEVICE_CLASS(class);
	pci_class->realize = pci_jeriesdev_init;
	pci_class->exit = pci_jeriesdev_uninit;
	/*Information to Identify Device*/
	pci_class->device_id = 0x0001;
	pci_class->vendor_id = 0x1337;
	pci_class->class_id = PCI_CLASS_OTHERS;
	set_bit(DEVICE_CATEGORY_MISC, dc->categories);
	pci_class->revision = 0x00;
	dc->desc = "PCI Jeries Driver";
	dc->reset = qdev_pci_jeriesdev_reset;
	
}
/*
pci_jeries_info: structure that describes device in terms of name, device's parent, initial configuration function and instance size.
*/
static const TypeInfo pci_jeries_info = {
	.name = TYPE_PCI_JERIES_DEV,
	.parent = TYPE_PCI_DEVICE,
	.instance_size = sizeof(PCIJeriesDevState),
	.class_init = pci_jeriesdev_class_init,
};
/*
Function to register device types
*/
static void pci_jeries_register_types(void){
	type_register_static(&pci_jeries_info);
}
/*Register my device in QEMU*/
type_init(pci_jeries_register_types);
