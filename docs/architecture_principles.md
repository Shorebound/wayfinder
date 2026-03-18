# Architecture Principles

## The Engine is a library, not an application

The game (and later, the editor) are consumers of the engine. The engine never knows it's being used by an editor.

## Data flows down, events flow up. 

Systems read data and produce data. Side effects are explicit and channeled through an event bus or command queue.

## If it can be a file on disk, it should be. 

Materials, entity archetypes, input mappings, AI behavior trees, dialogue — all data, all loadable, all hot-reloadable.