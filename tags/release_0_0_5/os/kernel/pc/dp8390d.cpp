/*
 * Copyright (c) 2006
 * Nintendo Co., Ltd.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Nintendo makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */
/*
 * These coded instructions, statements, and computer programs contain
 * software derived from the following specification:
 *
 * National Semiconductor, "DP8390D/NS32490D NIC Network Interface Controller", July 1995.
 * http://www.national.com/opf/DP/DP8390D.html
 */

#include <string.h>
#include <errno.h>
#include <es/types.h>
#include <es/formatter.h>
#include <es/clsid.h>
#include <es/exception.h>
#include "i386/dp8390d.h"
#include "i386/io.h"
#include "i386/core.h"

int Dp8390d::
readProm()
{
    Lock::Synchronized method(spinLock);

    unsigned char buf[32];
    readNicMemory(0, buf, sizeof(buf));

    // check data bus type.
    switch (buf[0x1c])
    {
      case 'W': // OK
#ifdef VERBOSE
        esReport("data bus: 16-bit\n");
#endif // VERBOSE
        break;

      case 'B':
#ifdef VERBOSE
        esReport("data bus: 8-bit\n");
#endif // VERBOSE
        // FALL THROUGH
      default:
        esReport("Unsupported data bus.\n");
        return -1;
    }

    // copy mac address.
    int i;
    for (i = 0; i < 6; ++i)
    {
        mac[i] = buf[2*i];
    }

#ifdef VERBOSE
    esReport("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif // VERBOSE

    return 0;
}

int Dp8390d::
initializeRing()
{
#ifdef VERBOSE
    esReport("txPageStart    : %d\n", txPageStart);
    esReport("ring           : %d - %d\n", ring.pageStart, ring.pageStop);
    esReport("ring.nextPacket: %d\n", ring.nextPacket);
#endif // VERBOSE

    unsigned char buf[PAGE_SIZE];
    memset(buf, 0, sizeof(buf));
    int page;

    for (page = ring.pageStart; page < ring.pageStop; ++page)
    {
        writeToNicMemory(page * PAGE_SIZE, buf, sizeof(buf));
    }

    {
        Lock::Synchronized method(spinLock);

        outpb(base + PSTART, ring.pageStart);
        outpb(base + PSTOP, ring.pageStop);
        outpb(base + BNRY, ring.pageStart);

        // Program Command Register for page 1
        outpb(base + CR, CR_STP | CR_RD2 | CR_PAGE1);

        outpb(base + CURR, ring.nextPacket);

        // Program Command Register for page 0
        outpb(base + CR, CR_STP | CR_RD2 | CR_PAGE0);

        ring.nextPacket = ring.pageStart + 1;
    }
    return 0;
}

int Dp8390d::
initializeMacAddress()
{
    Lock::Synchronized method(spinLock);

    unsigned char cr = setPage(CR_PAGE1);

    outpb(base + PAR0, mac[0]);
    outpb(base + PAR1, mac[1]);
    outpb(base + PAR2, mac[2]);
    outpb(base + PAR3, mac[3]);
    outpb(base + PAR4, mac[4]);
    outpb(base + PAR5, mac[5]);

    restorePage(cr);

    return 0;
}

int Dp8390d::
initializeMulticastAddress()
{
    Lock::Synchronized method(spinLock);

    // Program Command Register for page 1
    unsigned char cr = setPage(CR_PAGE1);

    int i;
    for (i = 0; i < NUM_HASH_REGISTER; ++i)
    {
        outpb(base + MAR0 + i, 0);
    }

    memset(hashTable, 0, sizeof(hashTable));
    memset(hashRef, 0, sizeof(hashRef));

    // Program Command Register for page 0
    restorePage(cr);

    rcr = RCR_MON | RCR_AB | RCR_AM;
    outpb(base + RCR, rcr);
    return 0;
}

/*
 * DP8390D/NS32490D NIC Network Interface Controller
 *
 * Initialization Sequence
 * The following initialization procedure is mandatory.
 * - reset()
 *     1) Program Command Register for Page 0 (Command Register 21H)
 *     2) Initialize Data Configuration Register (DCR)
 *     3) Clear Remote Byte Count Registers (RBCR0, RBCR1)
 *     4) Initialize Receive Configuration Register (RCR)
 *     5) Place the NIC in LOOPBACK mode 1 or 2 (Transmit Configuration Register e 02H or 04H)
 *     6) Initialize Receive Buffer Ring: Boundary Pointer (BNDRY), Page Start (PSTART), and Page Stop (PSTOP)
 *     7) Clear Interrupt Status Register (ISR) by writing 0FFh to it.
 *     8) Initialize Interrupt Mask Register (IMR)
 *
 *     9) Program Command Register for page 1 (Command Register 61H)
 * - initializeMacAddress()
 *         i)Initialize Physical Address Registers (PAR0-PAR5)
 * - initializeMulticastAddress()
 *         ii)Initialize Multicast Address Registers (MAR0-MAR7)
 * - initializeRing()
 *         iii)Initialize CURRent pointer
 *
 * - start()
 *     10) Put NIC in START mode (Command Register e 22H).
 *         The local receive DMA is still not active since the NIC is in LOOPBACK.
 *     11) Initialize the Transmit Configuration for the intended value.
 *
 *     The NIC is now ready for transmission and reception.
 */

int Dp8390d::
reset()
{
    {
        Lock::Synchronized method(spinLock);

        // Program Command Register for page 0
        outpb(base + CR, CR_STP | CR_RD2 | CR_PAGE0);

        // Initialize Data Configuration Register (DCR)
        outpb(base + DCR, DCR_FT1 | DCR_LS); // for PCI device

        // Clear Remote Byte Count Registers (RBCR0, RBCR1)
        outpb(base + RBCR0, 0);
        outpb(base + RBCR1, 0);

        // Initialize Receive Configuration Register (RCR)
        // check addresses and CRC on incoming packets without buffering to memory.
        // accept broadcast.
        rcr = RCR_MON | RCR_AB;
        outpb(base + RCR, rcr);

        // Place the NIC in loopback mode.
        outpb(base + TCR, TCR_LB0);
    }

    // Initialize Receive Buffer Ring: Boundary Pointer (BNDRY), Page Start (PSTART), and Page Stop (PSTOP)
    initializeRing();

    {
        Lock::Synchronized method(spinLock);

        outpb(base + TPSR, txPageStart);

        // Clear Interrupt Status Register (ISR) by writing 0FFh to it.
        outpb(base + ISR, 0xff);

        // Initialize Interrupt Mask Register
        outpb(base + IMR, IMR_CNTE | IMR_OVWE | IMR_TXEE | IMR_RXEE | IMR_PTXE | IMR_PRXE);
    }

    return 0;
}

int Dp8390d::
writeLocked(const void* src, int count)
{
    writeToNicMemory(txPageStart * PAGE_SIZE, (unsigned char*) src, count);

    {
        Lock::Synchronized method(spinLock);

        sendDone = false;
        outpb(base + TBCR0, count & 0xff);
        outpb(base + TBCR1, (count >> 8) & 0xff);
        outpb(base + CR, CR_RD2 | CR_TXP | CR_STA | CR_PAGE0);

        // update statistics.
        statistics.outOctets += (unsigned long long) count;

        if (*((unsigned char*) src) & 0x01)
        {
            ++statistics.outNUcastPkts;
        }
        else
        {
            ++statistics.outUcastPkts;
        }
    }

    return count;
}

int Dp8390d::
getPacketSize(RingHeader* header)
{
    Lock::Synchronized method(spinLock);

    /*
    In StarLAN applications using bus clock frequencies greater
    than 4 MHz, the NIC does not update the buffer header
    information properly because of the disparity between the
    network and bus clock speeds.
    */

    // calculate the upper byte count.
    int lenHigh;
    if (ring.nextPacket < header->nextPage)
    {
        lenHigh = header->nextPage - ring.nextPacket - 1;
    }
    else
    {
        lenHigh = ring.pageStop - ring.nextPacket
            + header->nextPage - ring.pageStart - 1;
    }

    if (header->lenLow > PAGE_SIZE - sizeof(RingHeader))
    {
        ++lenHigh;
    }

    return (lenHigh << 8) | header->lenLow - sizeof(RingHeader);
}

int Dp8390d::
checkRingStatus(RingHeader* header, int len)
{
    if (header->nextPage < ring.pageStart ||
        ring.pageStop <= header->nextPage ||
        len < MIN_SIZE || MAX_SIZE < len)
    {
#ifdef VERBOSE
        esReport("header->nextPage %d ring.pageStop %d len %d\n", len);
#endif // VERBOSE
        // reinitialize the ring.
        {
            Lock::Synchronized method(spinLock);
            outpb(base + CR, CR_RD2 | CR_STP | CR_PAGE0);
        }
        initializeRing();
        {
            Lock::Synchronized method(spinLock);
            outpb(base + CR, CR_RD2 | CR_STA | CR_PAGE0);
        }
        return -1;
    }

    return 0;
}

int Dp8390d::
updateRing(RingHeader* header)
{
    Lock::Synchronized method(spinLock);

    /*
     * After a packet is DMAed from the Receive Buffer Ring,
     * the Next Page Pointer (second byte in NIC buffer header)
     * is used to update BNDRY and nextPacket.
     */
    ring.nextPacket = header->nextPage;
    if (--header->nextPage < ring.pageStart)
    {
        header->nextPage = ring.pageStop - 1;
    }

    outpb(base + BNRY, header->nextPage);
}

bool Dp8390d::
isRingEmpty()
{
    return (getCurr() == ring.nextPacket);
}

void Dp8390d::
updateReceiveStatistics(RingHeader* header, int len)
{
    Lock::Synchronized method(spinLock);

    if (header->status & RSR_PRX)
    {
        statistics.inOctets += (unsigned long) len;

        if (header->status & RSR_PHY)
        {
            ++statistics.inNUcastPkts;
        }
        else
        {
            ++statistics.inUcastPkts;
        }
        return;
    }

    ++statistics.inDiscards;
    if (header->status & RSR_FAE)
    {
        ++statistics.inUnknownProtos;
    }
    else
    {
        ++statistics.inErrors;
    }
}

int Dp8390d::
readLocked(void* dst, int count)
{
    unsigned char* buf = static_cast<unsigned char*>(dst);
    int total = 0;
    RingHeader header;
    unsigned short nextPacketAddress;

    if (!isRingEmpty())
    {
        {
            Lock::Synchronized method(spinLock);

            nextPacketAddress = ring.nextPacket * PAGE_SIZE;
            // read a packet status.
            readNicMemory(nextPacketAddress, (unsigned char*) &header, sizeof(header));
        }
        int len = getPacketSize(&header);

        // check ring status.
        if (checkRingStatus(&header, len) < 0)
        {
#ifdef VERBOSE
            esReport("%s: ring status error.\n", __func__);
#endif // VERBOSE
            return -1;
        }

        updateReceiveStatistics(&header, len);

        // read data
        if ((header.status & (RSR_FO | RSR_FAE | RSR_CRC | RSR_PRX)) == RSR_PRX)
        {
            Lock::Synchronized method(spinLock);

            // This packet was received with no errors.
            if (count < len)
            {
#ifdef VERBOSE
                esReport("%s: The specified buffer is too small.\n", __func__);
#endif // VERBOSE
                return -1;
            }
            total += len;

            nextPacketAddress += sizeof(header);

            if (ring.pageStop * PAGE_SIZE <= nextPacketAddress + len)
            {
                int lenToTail;
                lenToTail = ring.pageStop * PAGE_SIZE - nextPacketAddress;
                readNicMemory(nextPacketAddress, buf, lenToTail);
                len -= lenToTail;
                count -= lenToTail;
                buf += lenToTail;
                nextPacketAddress = ring.pageStart * PAGE_SIZE;
            }

            if (0 < len)
            {
                readNicMemory(nextPacketAddress, buf, len);
            }
        }

        // update pointers.
        updateRing(&header);
    }

    return total;
}

unsigned int Dp8390d::
generateCrc(const unsigned char* mca)
{
    unsigned idx;
    unsigned bit;
    unsigned data;
    unsigned crc = 0xffffffff;

    for (idx = 0; idx < 6; idx++)
    {
        for (data = *mca++, bit = 0; bit < 8; bit++, data >>=1)
        {
            crc = (crc >> 1) ^ (((crc ^ data) & 1) ? POLY : 0);
        }
    }
    return crc;
}

void Dp8390d::
issueStopCommand()
{
    resend = inpb(base + CR) & CR_TXP; // store
    outpb(base + CR, CR_RD2 | CR_STP | CR_PAGE0); // issue stop command.
    overflow.exchange(true);
    lastOverflow = DateTime::getNow();
}

int Dp8390d::
recoverFromOverflow()
{
    unsigned char cr;
    {
        Lock::Synchronized method(spinLock);

        TimeSpan wait(0, 0, 0, 0, 2);
        if (DateTime::getNow() < lastOverflow + wait)
        {
            return -1;
        }

        cr = inpb(base + CR);

        // Clear Remote Byte Count Registers.
        outpb(base + RBCR0, 0);
        outpb(base + RBCR1, 0);

        if (resend && (inpb(base + ISR) & (ISR_PTX | ISR_TXE)))
        {
            resend = false;
        }

        outpb(base + TCR, TCR_LB0); // loopback mode.
        outpb(base + CR, CR_RD2 | CR_STA | CR_PAGE0); // Issue start command.

        // Remove one or more packets from the receive buffer ring.
        ring.nextPacket = ring.pageStart + 1;
    }

    initializeRing();

    {
        Lock::Synchronized method(spinLock);

        outpb(base + ISR, ISR_OVW);
        outpb(base + TCR, 0);

        if(resend)
        {
            outpb(base + CR, CR_RD2 | CR_TXP | CR_STA | CR_PAGE0);
        }
    }

    overflow.exchange(false);

    return 0;
}

int Dp8390d::
readNicMemory(unsigned short src, unsigned char* buf, unsigned short len)
{
    unsigned char cr = inpb(base + CR) & ~CR_TXP; // save

    // select page 0 registers
    outpb(base + CR, CR_RD2 | CR_STA | CR_PAGE0);

    // set up DMA byte count
    outpb(base + RBCR0, 0xff & len);
    outpb(base + RBCR1, len >> 8);

    // set up source address in NIC memory.
    outpb(base + RSAR0, 0xff & src);
    outpb(base + RSAR1, src >> 8);

    outpb(base + CR, CR_RD0 | CR_STA | CR_PAGE0);

    inpsb(base + DATA, buf, len);

    outpb(base + ISR, ISR_RDC);

    outpb(base + CR, cr); // restore

    return len;
}

int Dp8390d::
writeToNicMemory(unsigned short dst, unsigned char* buf, unsigned short len)
{
    {
        Lock::Synchronized method(spinLock);
        // select page 0 registers
        outpb(base + CR, CR_RD2 | CR_STA | CR_PAGE0);

        // reset remote DMA complete flag
        outpb(base + ISR, ISR_RDC);

        // set up DMA byte count
        outpb(base + RBCR0, len);
        outpb(base + RBCR1, len >> 8);

        // set up destination address in NIC memory.
        outpb(base + RSAR0, 0xff & dst);
        outpb(base + RSAR1, dst >> 8);

        // set remote DMA write
        outpb(base + CR, CR_RD1 | CR_STA | CR_PAGE0);

        outpsb(base + DATA, buf, len); // Data port
    }

    // Wait for remote DMA complete.
    while ((getIsr() & ISR_RDC) != ISR_RDC)
    {
        monitor->lock();
        monitor->wait();
        monitor->unlock();
    }

    return 0;
}

unsigned char Dp8390d::
setPage(int page)
{
    // page: CR_PAGE[0-2]
    unsigned char cr = inpb(base + CR) & ~CR_TXP;
    outpb(base + CR, (cr & ~(CR_PS0 | CR_PS1)) | page);
    return cr;
}

unsigned char Dp8390d::
getIsr()
{
    Lock::Synchronized method(spinLock);
    return inpb(base + ISR);
}

unsigned char Dp8390d::
getCurr()
{
    unsigned char cr = setPage(CR_PAGE1);
    unsigned char curr = inpb(base + CURR);
    restorePage(cr);
    return curr;
}

void Dp8390d::
restorePage(unsigned char cr)
{
    outpb(base + CR, cr);
}

//
// IEthernet
//

Dp8390d::
Dp8390d(unsigned base, int irq) : ref(0), base(base), irq(irq), sendDone(false),
    overflow(false), lastOverflow(0)
{
    esCreateInstance(CLSID_Monitor,
                     IID_IMonitor,
                     reinterpret_cast<void**>(&monitor));

    /*
     * Reset the board. This is done by doing a read
     * followed by a write to the Reset address.
     */
    unsigned char tmp = inpb(base + RESET);
    esSleep(20000);    // wait for 2 milliseconds
    outpb(base + RESET, tmp);
    esSleep(20000);

    if (probe() < 0)
    {
        // DP8390D is not found, or the on-board memory is not available.
        throw SystemException<ENODEV>();
    }

    esReport("Ethernet adapter: DP8390D (IRQ %d, I/O 0x%02x)\n", irq, base);

    // buffers in the NIC memory.
    txPageStart = reservedPage / PAGE_SIZE;
    ring.pageStart = txPageStart + NUM_TX_PAGE;
    ring.pageStop = txPageStart + (nicMemSize - reservedPage) / PAGE_SIZE;
    ring.nextPacket = ring.pageStart + 1;
    reset();

    Core::registerExceptionHandler(32 + irq, this);

    // read PROM emulation area of NIC memory.
    if (readProm() < 0)
    {
        return;
    }

    // set mac address to registers (PAR0-5).
    initializeMacAddress();
    initializeMulticastAddress();

    memset(&statistics, 0, sizeof(statistics));
}

Dp8390d::~Dp8390d()
{
    Lock::Synchronized method(spinLock);

    if (monitor)
    {
        monitor->release();
    }
}

int Dp8390d::
start()
{
    if (overflow && recoverFromOverflow() < 0)
    {
        return -1;
    }

    {
        Lock::Synchronized method(spinLock);
        // Put NIC in START mode (Command Register 22H).
        outpb(base + CR, CR_RD2 | CR_STA | CR_PAGE0);

        // Initialize TCR to be ready for transmission and reception.
        outpb(base + TCR, 0);
    }
}

int Dp8390d::
stop()
{
    {
        Lock::Synchronized method(spinLock);

        // stop command.
        outpb(base + CR, CR_RD2 | CR_STP | CR_PAGE0);
        outpb(base + RBCR0, 0);
        outpb(base + RBCR1, 0);
    }

    int timeout;
    for (timeout = 10; (getIsr() & ISR_RST) == 0 && timeout; --timeout)
    {
        esSleep(2000);
    }

    {
        Lock::Synchronized method(spinLock);
        // Place the NIC in loopback mode.
        outpb(base + TCR, TCR_LB0);
    }

    return 0;
}

int Dp8390d::
probe()
{
    {
        Lock::Synchronized method(spinLock);

        outpb(base + CR, CR_RD2 | CR_STP | CR_PAGE0);

        if ((inpb(base + CR) & (CR_RD2 | CR_TXP | CR_STA | CR_STP)) != (CR_RD2 | CR_STP))
        {
            return -1;
        }

        if ((inpb(base + ISR) & ISR_RST) != ISR_RST)
        {
            return -1;
        }

        // This Ethernet adapter is DS8390.
        // assume on-board memory size.
        nicMemSize = 32 * 1024;
        reservedPage = 16 * 1024;

        // Temporarily initialize DCR for test.
        outpb(base + DCR, DCR_FT1 | DCR_LS); // for PCI device
        outpb(base + PSTART, reservedPage / PAGE_SIZE);
        outpb(base + PSTOP, nicMemSize / PAGE_SIZE);
        outpb(base + BNRY, reservedPage / PAGE_SIZE);
    }

    // check if the parameters are valid.
    unsigned char testPattern[] = "Write this pattern, then read the memory and compare them.";
    unsigned char buf[64];
    memset(buf, 0, sizeof(buf));

    writeToNicMemory(reservedPage, testPattern, sizeof(testPattern));

    {
        Lock::Synchronized method(spinLock);
        readNicMemory(reservedPage, buf, sizeof(buf));
    }
    if (memcmp(testPattern, buf, sizeof(testPattern)) != 0)
    {
        return -1;
    }

    return 0;
}

bool Dp8390d::
getPromiscuousMode()
{
    Lock::Synchronized method(spinLock);

    return rcr & RCR_PRO;
}

void Dp8390d::
setPromiscuousMode(bool on)
{
    Lock::Synchronized method(spinLock);

    if (on == (rcr & RCR_PRO))
    {
        return;
    }

    unsigned char cr = setPage(CR_PAGE0);
    if (on)
    {
        rcr = (rcr | RCR_PRO) & ~RCR_AM;
        outpb(base + RCR, rcr);

        // Program Command Register for page 1
        setPage(CR_PAGE1);
        int i;
        for (i = 0; i < NUM_HASH_REGISTER; ++i)
        {
            outpb(base + MAR0 + i, 0xff);
        }
    }
    else
    {
        rcr = (rcr & ~RCR_PRO) | RCR_AM;
        outpb(base + RCR, rcr);

        // Program Command Register for page 1
        setPage(CR_PAGE1);

        int i;
        for (i = 0; i < NUM_HASH_REGISTER; ++i)
        {
            outpb(base + MAR0 + i, hashTable[i]);
        }
    }

    restorePage(cr);
}

int Dp8390d::
addMulticastAddress(unsigned char macaddr[6])
{
    Lock::Synchronized method(spinLock);
    unsigned char* multicast = macaddr;
    if (!(*multicast & 0x01))
    {
        return -1;
    }

    // Get the 6 most significant bits (little endian).
    int msb = 0x3f & generateCrc(multicast);
    msb = ((msb & 1) << 5) | ((msb & 2) << 3) |
          ((msb & 4) << 1) | ((msb & 8) >> 1) |
          ((msb & 16) >> 3) | ((msb & 32) >> 5);

    // increment reference count.
    if (++hashRef[msb] == 1)
    {
        // Program Command Register for page 1
        unsigned char cr = setPage(CR_PAGE1);

        unsigned char mar = inpb(base + MAR0 + msb/8);
        unsigned char bit = 1<<(msb % 8);

        if (!(mar & bit))
        {
            mar |= bit;
            outpb(base + MAR0 + msb/8, mar);
            hashTable[msb/8] = mar;
        }

        restorePage(cr);
    }

    return 0;
}

int Dp8390d::
removeMulticastAddress(unsigned char macaddr[6])
{
    Lock::Synchronized method(spinLock);

    unsigned char* multicast = macaddr;
    if (!(*multicast & 0x01))
    {
        return -1;
    }

    // Get the 6 most significant bits.
    int msb = 0x3f & generateCrc(multicast);
    msb = ((msb & 1) << 5) | ((msb & 2) << 3) |
          ((msb & 4) << 1) | ((msb & 8) >> 1) |
          ((msb & 16) >> 3) | ((msb & 32) >> 5);

    if (--hashRef[msb] <= 0)
    {
        hashRef[msb] = 0;

        // Program Command Register for page 1
        unsigned char cr = setPage(CR_PAGE1);
        unsigned char mar = inpb(base + MAR0 + msb/8);
        unsigned char bit = 1<<(msb % 8);

        if (mar & bit)
        {
            mar &= ~bit;
            outpb(base + MAR0 + msb/8, mar);
            hashTable[msb/8] = mar;
        }

        restorePage(cr);
    }

    return 0;
}

void Dp8390d::
getMacAddress(unsigned char mac[6])
{
    Lock::Synchronized method(spinLock);
    memmove(mac, this->mac, sizeof(this->mac));
}

bool Dp8390d::
getLinkState()
{
    return true;
}

int Dp8390d::
getMode()
{
    return IEthernet::MODE_10FULL;
}

void Dp8390d::
getStatistics(InterfaceStatistics* statistics)
{
    Lock::Synchronized method(spinLock);

    *statistics = this->statistics;
};

//
// IStream
//
int Dp8390d::
read(void* dst, int count)
{
    if (overflow && recoverFromOverflow() < 0)
    {
        return -1;
    }

    int ret;

    monitor->lock();
    while ((ret = readLocked(dst, count)) == 0)
    {
        monitor->wait();
    }
    monitor->unlock();
    return ret;
}

int Dp8390d::
write(const void* src, int count)
{
    if (overflow && recoverFromOverflow() < 0)
    {
        return -1;
    }

    if (!src || count < 0 || MAX_SIZE < count)
    {
        return -1;
    }

    monitor->lock();
    int ret = writeLocked(src, count);

    while (!sendDone)
    {
        monitor->wait();
    }
    monitor->unlock();

    return ret;
}

//
// ICallback
//
int  Dp8390d::
invoke(int irq)
{
    Lock::Synchronized method(spinLock);

    outpb(base + IMR, 0x00); // disable interrupts.

    unsigned char isr;
    while ((isr = inpb(base + ISR)) & (ISR_CNT | ISR_OVW | ISR_TXE | ISR_RXE | ISR_PTX | ISR_PRX))
    {
        if (isr & (ISR_TXE | ISR_PTX))
        {
            // TRANSMIT ERROR or PACKET TRANSMITTED
            // update statistics
            if (isr & ISR_TXE)
            {
                ++statistics.outDiscards;
                ++statistics.outCollisions;
            }

            outpb(base + ISR, ISR_TXE | ISR_PTX); // clear.
            sendDone = true;
            monitor->notifyAll();
        }

        if (isr & ISR_OVW)
        {
            // OVERWRITE WARNING: receive buffer ring storage resources have
            // been exhausted. (Local DMA has reached Boundary Pointer).
            ++statistics.inDiscards;
            issueStopCommand();
            outpb(base + ISR, ISR_OVW); // clear.
        }

        if (isr & (ISR_RXE | ISR_PRX))
        {
            // RECEIVE ERR or PACKET RECEIVED
            // An error is written into the ring header.
            outpb(base + ISR, ISR_RXE | ISR_PRX); // clear.
            monitor->notifyAll();
        }
    }

    if (isr & ISR_RDC)
    {
        monitor->notifyAll();
        outpb(base + ISR, ISR_RDC); // clear.
    }

    outpb(base + IMR, (IMR_CNTE | IMR_OVWE | IMR_TXEE | IMR_RXEE | IMR_PTXE | IMR_PRXE));

    return 0;
}

//
// IInterface
//

bool Dp8390d::
queryInterface(const Guid& riid, void** objectPtr)
{
    if (riid == IID_IStream)
    {
        *objectPtr = static_cast<IStream*>(this);
    }
    else if (riid == IID_IEthernet)
    {
        *objectPtr = static_cast<IEthernet*>(this);
    }
    else if (riid == IID_IInterface)
    {
        *objectPtr = static_cast<IEthernet*>(this);
    }
    else
    {
        *objectPtr = NULL;
        return false;
    }
    static_cast<IInterface*>(*objectPtr)->addRef();
    return true;
}

unsigned int Dp8390d::
addRef()
{
    return ref.addRef();
}

unsigned int Dp8390d::
release()
{
    unsigned long count = ref.release();
    if (count == 0)
    {
        delete this;
        return 0;
    }
    return count;
}