#include "World/Items/GvTLockpickItem.h"

AGvTLockpickItem::AGvTLockpickItem()
{
	ItemPurpose = EGvTItemPurpose::Equipment;
	bUpsetsGhostsOnInteract = false;
	bTreatAsValuableForGhosts = false;
}
