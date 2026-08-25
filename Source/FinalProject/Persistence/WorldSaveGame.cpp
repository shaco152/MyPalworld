#include "Persistence/WorldSaveGame.h"

bool UWorldSaveGame::IsStructurallyValid() const
{
	return SchemaVersion > 0 && SchemaVersion <= CurrentSchemaVersion && Metadata.WorldId.IsValid() &&
		Metadata.SaveRevision >= 0 && !Metadata.MapPath.IsEmpty();
}
