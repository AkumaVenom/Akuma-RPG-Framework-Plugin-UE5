from dataclasses import dataclass

@dataclass
class Slot:
    item: str = ''
    inst: str = ''
    rev: int = 0


def canonical(slots, allow_same_type=False, preferred=0):
    order = list(range(len(slots)))
    order.sort(key=lambda i: (0 if i + 1 == preferred else 1, -slots[i].rev, i))
    seen_items=set(); seen_inst=set()
    for i in order:
        s=slots[i]
        if not s.item and not s.inst:
            continue
        if (not allow_same_type and s.item in seen_items) or (s.inst and s.inst in seen_inst):
            slots[i]=Slot()
            continue
        if s.item: seen_items.add(s.item)
        if s.inst: seen_inst.add(s.inst)
    return slots

def assign(slots, target, item, inst, next_rev, allow_same_type=False):
    ti=target-1
    for i,s in enumerate(slots):
        if i==ti: continue
        if (s.inst and s.inst==inst) or (not allow_same_type and s.item==item):
            slots[i]=Slot()
    slots[ti]=Slot(item,inst,next_rev)
    return canonical(slots, allow_same_type, target)

# move same exact axe 1 -> 4 -> 7; only newest target survives.
slots=[Slot() for _ in range(8)]
slots=assign(slots,1,'StoneAxe','GUID-A',1)
slots=assign(slots,4,'StoneAxe','GUID-A',2)
slots=assign(slots,7,'StoneAxe','GUID-A',3)
assert [i+1 for i,s in enumerate(slots) if s.inst=='GUID-A']==[7]

# Legacy duplicate state: preferred/newest target wins repair.
slots=[Slot('StoneAxe','GUID-A',0), Slot(), Slot('StoneAxe','GUID-A',0), Slot('StoneAxe','GUID-A',5)]
slots=canonical(slots, False, 4)
assert [i+1 for i,s in enumerate(slots) if s.inst=='GUID-A']==[4]

# Same type may use different runtime instances only when explicitly enabled.
slots=[Slot() for _ in range(4)]
slots=assign(slots,1,'Potion','GUID-P1',1,True)
slots=assign(slots,2,'Potion','GUID-P2',2,True)
assert [i+1 for i,s in enumerate(slots) if s.item=='Potion']==[1,2]
slots=assign(slots,4,'Potion','GUID-P1',3,True)
assert [i+1 for i,s in enumerate(slots) if s.inst=='GUID-P1']==[4]
assert [i+1 for i,s in enumerate(slots) if s.inst=='GUID-P2']==[2]

print('quick-access duplicate model: PASS')
