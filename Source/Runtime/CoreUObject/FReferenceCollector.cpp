#include "FReferenceCollector.h"
#include "UObject.h"

void FReferenceCollector::AddReferencedObject(UObject* Object)
{
	if (Object == nullptr) return;

	if (ReferencedObjects.insert(Object).second == true)
		PendingObjects.push_back(Object);
}

void FReferenceCollector::ProcessReferences()
{
	while (!PendingObjects.empty()) {
		UObject* Object = PendingObjects.back();
		PendingObjects.pop_back();

		Object->AddReferencedObjects(*this);
	}
}

bool FReferenceCollector::bIsReferenced(const UObject* Object) const
{
	return ReferencedObjects.contains(Object);
}
