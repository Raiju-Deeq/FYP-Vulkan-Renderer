
### Candidate Research Gaps in Neural Rendering (with pointers into the literature)

Below are several concrete “gap directions” that are poorly covered or explicitly flagged as open problems in the survey and method papers you provided, plus how they connect to each other. Each could be scoped into a PhD topic.

---

## 1. Physically‑based neural rendering with full transport, at real‑time rates

Recent work pushes 3D/4D Gaussian splatting to real‑time novel‑view synthesis but is largely limited to _view interpolation under fixed lighting_ and simplified radiance models (Kerbl et al., 2023). Surveys on differentiable rendering explicitly note that physics‑based differentiable rendering (full light transport, complex BRDFs, global illumination) is much less developed than NeRF/3DGS‑style methods and highlight open problems there (Gao & Qi, 2024).

“Relightable 3D Gaussians” adds BRDF and ray‑traced shadows on top of Gaussians, but remains relatively heavy and focused on offline‑style, point‑based relighting (Gao et al., 2024). A survey of 3D Gaussian splatting emphasizes its strength in fast rendering and editing, but mainly for direct‑illumination style appearance (Wu et al., 2024).

**Gap direction:** unified, _physically‑based_ neural rendering that:

- Uses explicit primitives like Gaussians/points for fast rasterization,
- But couples them to a principled transport solver (importance‑sampled path tracing, transient transport, participating media),
- And runs close to real‑time for at least small scenes.

You could position this directly at the intersection of PBDR and 3DGS/NeRF, which the PBDR survey calls out as lacking mature methods and benchmarks (Gao & Qi, 2024).

---

## 2. End‑to‑end hardware–algorithm co‑design for neural rendering

The hardware‑oriented review stresses that current neural rendering workloads (MLPs, ray marching, hash tables, etc.) stress conventional GPUs in both compute and memory, and argues for specialized architectures but notes open design challenges (Yan et al., 2024).

At the same time, NeRF/3DGS work is moving aggressively toward real‑time and mobile (e.g., MobileNeRF, hash‑encoded instant NGP, memory‑efficient radiance fields) (Wu et al., 2024; Yang, 2024). Reviews of real‑time NeRFs emphasize that achieving desktop‑class quality and frame rate on mobile remains difficult due to memory and compute constraints (Yang, 2024).

**Gap direction:** algorithm/representation designs that are _co‑designed_ with a specific accelerator model:

- Tailoring neural primitives (Gaussians, voxels, low‑rank fields) to map cleanly to tiled on‑chip SRAM and fixed‑function units.
- Minimizing random memory access and branching in ray marching, which current surveys identify as hardware pain points (Yan et al., 2024).
- Possibly defining a simple ISA or “NRPU”‑like abstraction as argued in the hardware review (Yan et al., 2024).

This is attractive as a PhD topic because it combines systems and graphics and is repeatedly flagged as an unsolved challenge.

---

## 3. Neural rendering under extreme resource constraints and large‑scale scenes

NeRF‑like reviews repeatedly note that while implicit fields reduce storage, scaling to _large_ scenes and mobile devices remains an open problem; storage and memory bandwidth are highlighted as key obstacles for real‑time, high‑fidelity content in games/VR (Yang, 2024). Memory‑efficient MERF/SMERF address large unbounded scenes but still target desktop‑class GPUs (Yang, 2024).

Surveys of 3D Gaussian splatting also stress that explicit Gaussians are powerful but incur high memory in dense scenes, and they catalog editing and reconstruction works rather than principled multiscale compression (Wu et al., 2024).

**Gap direction:** multi‑resolution, _streamable_ neural scene representations for:

- City‑scale or game‑world‑scale content,
- On mobile/standalone headsets,
- With explicit quality–bandwidth–latency trade‑offs (analogous to video codecs).

This could sit at the intersection of variable‑bitrate neural fields and memory‑efficient NeRFs, but with a stronger information‑theoretic and systems flavor (e.g., online refinement, foveated field representations).

---

## 4. Semantically‑aware, editable neural scene graphs for interactive applications

The semantics‑focused NeRF survey points out that most prior NeRF and neural rendering surveys cover geometry and appearance, but only “superficial attention” is given to semantics and object‑level structure (Nguyen et al., 2024). This newer survey emphasizes viewpoint‑invariant semantic fields and object‑centric modeling as central to scene understanding and editing, yet still describes the area as emerging (Nguyen et al., 2024).

At the same time, 3DGS literature shows that explicit Gaussians are already effective for scene editing and simulation (Wu et al., 2024).

**Gap direction:** a unified, _semantic neural scene graph_:

- Nodes are object‑ or part‑level neural primitives (Gaussians, SDFs, meshes),
- Edges encode physical or functional relations,
- Supports real‑time rendering plus high‑level editing (move objects, change categories, relight only certain classes),
- Trained from sparse, weakly‑labeled or self‑supervised data.

Such a system would tie together semantically‑aware NeRFs and explicit neural primitives, going beyond per‑pixel semantics to structured, interactive scenes.

---

## 5. Dynamic scenes: robust, editable 4D neural primitives beyond interpolation

4D Gaussian splatting achieves very high FPS for dynamic view synthesis with decomposed 4D neural voxels and per‑timestamp deformations (Wu et al., 2024). Dynamic NeRF variants are surveyed extensively, focusing on modeling motion and achieving real‑time rates, but they largely treat motion as a nuisance rather than a controllable object (Liao et al., 2025).

**Gap direction:** 4D neural primitives that:

- Support _controllable_ motion (edit trajectories, re‑time animations),
- Factor out scene topology and articulation (e.g., re‑target a learned motion to new avatars),
- Maintain real‑time rendering and training efficiency.

This sits between classical graphics animation and 4D neural fields, and is under‑served in current surveys which focus on reconstruction and interpolation rather than controllable dynamics.

---

## 6. Methodological gap you can explicitly leverage in your literature review

Multiple surveys remark that prior NeRF/neural rendering surveys either:

- Focus on early NeRF and miss recent advances, or
- Provide broad coverage but “superficial” analysis, especially on semantics and scene understanding (Rabby & Zhang, 2023; Nguyen et al., 2024).

You can turn this into a _survey contribution plus technical thesis_: e.g., “Physically‑Based, Semantically‑Structured Neural Rendering: A Survey and New Representation”, where your first paper is an integrative, critical survey bridging PBDR, semantics, and Gaussian/NeRF methods, followed by your own model that fills one of the gaps above.

---

### How to choose among these

- If you like **graphics and physics**, Gap 1 or 5 is natural.
- If you like **systems/architecture**, Gap 2 or 3 plays to that.
- If you like **vision + graphics + interaction**, Gap 4 is promising and timely.

Each of these directions is repeatedly described as challenging or under‑explored in the reviews you are already reading, which makes them defensible starting points for a PhD thesis proposal.

_These search results were found and analyzed using Consensus, an AI-powered search engine for research. Try it at [https://consensus.app](https://consensus.app). © 2026 Consensus NLP, Inc. Personal, non-commercial use only; redistribution requires copyright holders’ consent._

## References

Kerbl, B., Kopanas, G., Leimkuehler, T., & Drettakis, G. (2023). 3D Gaussian Splatting for Real-Time Radiance Field Rendering. _ACM Transactions on Graphics (TOG)_, 42, 1 - 14. [https://doi.org/10.1145/3592433](https://doi.org/10.1145/3592433)

Wu, T., Yuan, Y., Zhang, L., Yang, J., Cao, Y., Yan, L., & Gao, L. (2024). Recent advances in 3D Gaussian splatting. _Computational Visual Media_, 10, 613 - 642. [https://doi.org/10.1007/s41095-024-0436-y](https://doi.org/10.1007/s41095-024-0436-y)

Liao, Y., Di, Y., Zhou, H., Zhu, K., Lu, M., Duan, Q., & Liu, J. (2025). A Survey on Neural Radiance Fields. _ACM Computing Surveys_, 58, 1 - 33. [https://doi.org/10.1145/3758085](https://doi.org/10.1145/3758085)

Gao, R., & Qi, Y. (2024). A Brief Review on Differentiable Rendering: Recent Advances and Challenges. _Electronics_. [https://doi.org/10.3390/electronics13173546](https://doi.org/10.3390/electronics13173546)

Yang, S. (2024). NeRF-based Real-Time Rendering Photo-Realistic Graphics Methods Review. _Transactions on Computer Science and Intelligent Systems Research_.

Yan, X., Xu, J., Huo, Y., & Bao, H. (2024). Neural Rendering and Its Hardware Acceleration: A Review. _none_.

Rabby, A., & Zhang, C. (2023). BeyondPixels: A Comprehensive Review of the Evolution of Neural Radiance Fields. _J. ACM_.

Nguyen, T., Bourki, A., Macudzinski, M., Brunel, A., & Bennamoun, M. (2024). Semantically-aware Neural Radiance Fields for Visual Scene Understanding: A Comprehensive Review. _arXiv_.

Gao, J., Gu, C., Lin, Y., Li, Z., Zhu, H., Cao, X., Zhang, L., & Yao, Y. (2024). Relightable 3D Gaussians: Realistic Point Cloud Relighting with BRDF Decomposition and Ray Tracing. _arXiv_.

Wu, G., Yi, T., Fang, J., Xie, L., Zhang, X., Wei, W., Liu, W., Tian, Q., & Wang, X. (2024). 4D Gaussian Splatting for Real-Time Dynamic Scene Rendering. _arXiv_.