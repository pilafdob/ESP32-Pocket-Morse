#include "../src/PairStore.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void storageTests() {
    Preferences::disk.clear(); Preferences::failWrites = false; Preferences::failOpen = false;
    uint8_t root[32]; memset(root, 7, sizeof(root));
    const uint8_t peer[6] = {2,3,4,5,6,7};
    PairStore first; assert(first.begin(0)); assert(!first.configured());
    assert(!first.nextTx() && !first.commitRx(1,true));
    assert(first.provision(peer,root)); assert(!first.provision(peer,root));
    assert(first.nextTx() == 1 && first.nextTx() == 2);
    const auto disk = Preferences::disk;
    assert(first.commitRx(10,false)); assert(Preferences::disk == disk);
    assert(first.commitRx(11,true));
    PairStore rebooted; assert(rebooted.begin(0));
    assert(rebooted.configured() && memcmp(rebooted.root(),root,32) == 0);
    assert(rebooted.lastRx() == 11 && rebooted.nextTx() == 257);
    assert(!rebooted.commitRx(11,true));
    Preferences::failWrites = true;
    assert(!rebooted.commitRx(12,true) && rebooted.lastRx() == 11);
    PairStore powerLoss; assert(powerLoss.begin(0)); assert(!powerLoss.nextTx());
    Preferences::failWrites = false;
    assert(powerLoss.nextTx() == 513);
    // Every acknowledged durable receive updates before the packet reaches App.
    assert(powerLoss.commitRx(100,true));
    PairStore again; assert(again.begin(0) && again.lastRx() == 100);
    Preferences::failOpen = true;
    PairStore unavailable; assert(!unavailable.begin(0) && !unavailable.nextTx());
    Preferences::failOpen = false;
    puts("PASS production PairStore: provisioning, counter reservation, reboot, durable receive, flash-wear policy and write failures");
}
