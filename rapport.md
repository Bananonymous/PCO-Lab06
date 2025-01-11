# Rapport du Laboratoire 06 : Gestion Concurrente de Thread Pools

### Auteurs : Nicolet Victor, Léon Surbeck

---

## Introduction

Le laboratoire 06 explore la gestion concurrente de ressources partagées à travers l'implémentation et les tests d'un **Thread Pool**. Ce composant permet de gérer dynamiquement un ensemble de threads pour exécuter des tâches en parallèle, tout en respectant des contraintes comme des limites de threads actifs, une gestion de file d'attente et des timeouts.

Les objectifs principaux sont :
1. Concevoir un **Thread Pool** capable de gérer efficacement les tâches en respectant les contraintes définies.
2. Évaluer sa robustesse et sa performance via une suite de tests couvrant divers scénarios.

---

## Choix d'implémentation

### Thread Pool

- **Structure du pool** :
    - La classe `ThreadPool` utilise une file d'attente pour gérer les tâches (`Runnable`) à exécuter.
    - Les threads inactifs sont mis en attente via des conditions (`Condition`), et de nouveaux threads sont créés uniquement si nécessaire, en respectant la limite maximale.

- **Gestion des tâches** :
    - Les tâches sont ajoutées via la méthode `start()` :
        - Si un thread inactif est disponible, il est réveillé.
        - Sinon, un nouveau thread est créé si la limite maximale n'est pas atteinte.
        - Si la file d'attente est pleine, la tâche est rejetée avec un appel à `cancelRun()`.

- **Arrêt du pool** :
    - La méthode `shutdown()` termine gracieusement tous les threads en attente ou actifs, en s'assurant qu'aucune tâche ne reste bloquée.

- **Timeout d'inactivité** :
    - Les threads inactifs sont automatiquement supprimés après un délai défini (`idleTimeout`), optimisant l'utilisation des ressources.

---

## Tests effectués

1. **Tests fonctionnels de base** :
    - Vérification de l'exécution correcte des tâches pour des tailles de pool variées.
    - Simulation de limites atteintes pour les threads actifs et la file d'attente.

2. **Tests de concurrence** :
    - Simulation d'appels simultanés à la méthode `start()` depuis plusieurs threads pour vérifier l'absence de deadlocks.
    - Vérification de l'exactitude des résultats dans des scénarios de concurrence élevée.

3. **Tests de robustesse** :
    - Simulation de cas extrêmes avec un grand nombre de tâches pour évaluer la gestion de la mémoire et la stabilité.
    - Validation de l'arrêt (`shutdown()`) pendant l'exécution de tâches actives.

4. **Tests de gestion dynamique** :
    - Test des threads inactifs supprimés après le timeout spécifié (`idleTimeout`).
    - Modification dynamique de la file d'attente et des priorités des tâches.

5. **Validation des erreurs** :
    - Vérification du comportement du pool pour des paramètres invalides (exemple : nombre de threads négatif).
    - Validation des rejets de tâches lorsque la capacité maximale est atteinte.

---

## Résultats

### Tests réussis
Les tests fonctionnels, de robustesse et de gestion dynamique montrent que :
- Le pool gère efficacement les tâches tout en respectant les contraintes.
- Les threads sont correctement créés et détruits en fonction des besoins et du timeout d'inactivité.
- Les cas limites, comme les files d'attente pleines ou des paramètres invalides, sont gérés sans plantage.

### Observations
- Les performances du pool sont optimales pour des charges modérées (~1000 tâches), mais une légère augmentation de la latence a été observée pour des charges très élevées (>10 000 tâches).
- Les threads inactifs sont bien supprimés après le timeout, montrant une gestion efficace des ressources.

---

## Conclusion

Le laboratoire 06 démontre l'efficacité d'un **Thread Pool** pour gérer des tâches de manière concurrente tout en optimisant les ressources. Les tests montrent que l'implémentation est robuste et répond aux exigences fonctionnelles. L'approche utilisée garantit une grande adaptabilité et peut facilement être étendue pour gérer des cas d'utilisation plus complexes.
