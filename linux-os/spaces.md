

The core division in modern operating systems:

### **Kernel Space**
- **Purpose**: Runs the OS kernel (core OS components)
- **Privileges**: Full hardware access, privileged instructions
- **Memory**: Protected memory area
- **Components**: Device drivers, memory management, scheduler, system calls
- **Stability**: Crash here = system crash (kernel panic)

### **User Space**
- **Purpose**: Runs applications and services
- **Privileges**: Restricted hardware access, uses system calls
- **Memory**: Isolated per process
- **Components**: Apps, libraries, user processes
- **Stability**: Crash here = usually just that app crashes

## **How They Interact**
```
User App → System Call Interface → Kernel → Hardware
```
Apps request kernel services via system calls (open files, network access, etc.)

## **Other Important "Spaces"**

### **3. Hardware Space**
- Physical CPU rings (x86: Ring 0-3)
- Ring 0: Kernel mode
- Ring 3: User mode

### **4. Address Space**
- Virtual memory layout per process
- Includes stack, heap, code segments

### **5. Container Namespaces** (Linux)
- PID namespace: Isolated process IDs
- Network namespace: Isolated network stack
- Mount namespace: Isolated filesystem
- IPC namespace: Isolated inter-process communication

### **6. Hypervisor Spaces**
- **Host Space**: Hypervisor/runs VMs
- **Guest Space**: Virtual machine's "kernel/user" spaces

## **Key Concept**
The **user/kernel split** enables:
- **Security**: Apps can't directly access hardware
- **Stability**: App crashes don't bring down OS
- **Multitasking**: Kernel manages resource sharing

Modern systems add more layers (containers, VMs) but this fundamental division remains.