Changes the way that areas outside the image are rendered by other nodes. 

By default, nodes that can render pixels outside of the original image (such as `Ripple Image` or `Resize Image`) will fill that background area with transparent black. If instead the output image from this node is fed into those nodes, those nodes can fill the background area differently. 

   - `Wrap Mode` — The way that background areas should be filled by nodes that use the output image. 
      - `None` — Fill with transparent black. 
      - `Clamp` — Fill by taking the colors around the edge of the image and extending them outward. 
      - `Repeat` — Fill by tiling the image. 
      - `Mirrored Repeat` — Fill by tiling the image, alternating between the original image and a flipped image. 
